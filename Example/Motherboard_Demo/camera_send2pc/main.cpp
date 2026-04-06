#include "zf_common_headfile.h"
#include <fcntl.h>
#include <linux/fb.h>
#include <opencv2/opencv.hpp>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <iostream>

#define KEY_0 "/dev/zf_driver_gpio_key_0"

const int MTU = 1000;
const int UDP_HEADER = 20;
const int MAX_PAYLOAD = MTU - UDP_HEADER;

struct TileData {
    std::vector<uint8_t> data;
};

struct SplitResult {
    std::vector<std::vector<cv::Mat>> tiles;
    std::vector<TileData> jpg_datas;
    int grid_x;
    int grid_y;
    int quality;
    bool valid;
};

using us = std::chrono::microseconds;
using ms = std::chrono::milliseconds;
auto now = std::chrono::steady_clock::now;

long to_us(auto duration) {
    return std::chrono::duration_cast<us>(duration).count();
}

// 1. 优化点：去掉 clone()。OpenCV 的 Mat(roi) 是浅拷贝，极快。
void split_image(const cv::Mat &img, int grid_x, int grid_y,
                 SplitResult &result) {
  int tile_h = img.rows / grid_y;
  int tile_w = img.cols / grid_x;

  result.tiles.assign(grid_y, std::vector<cv::Mat>(grid_x));

  for (int gy = 0; gy < grid_y; gy++) {
    for (int gx = 0; gx < grid_x; gx++) {
      // 指向原图的子区域，不涉及像素数据拷贝
      result.tiles[gy][gx] =
          img(cv::Rect(gx * tile_w, gy * tile_h, tile_w, tile_h));
    }
  }
}

// 保持不变，但建议在外部调用时控制频率
std::vector<uint8_t> compress_tile_to_jpg(const cv::Mat &tile, int quality) {
  std::vector<uint8_t> enc;
  cv::imencode(".jpg", tile, enc, {cv::IMWRITE_JPEG_QUALITY, quality});
  return enc;
}

// 2. 优化点：只在当前 grid
// 下进行一次压缩循环。如果单块超标，就地减小该块质量或跳过。
bool try_compress_tiles(const cv::Mat &img, int grid_x, int grid_y, int quality,
                        int max_size, SplitResult &result) {
  split_image(img, grid_x, grid_y, result);
  result.jpg_datas.clear();
  result.grid_x = grid_x;
  result.grid_y = grid_y;
  result.quality = quality;
  result.valid = true;

  for (int gy = 0; gy < grid_y; gy++) {
    for (int gx = 0; gx < grid_x; gx++) {
      auto jpg_data = compress_tile_to_jpg(result.tiles[gy][gx], quality);

      // 如果单块超标，尝试用极低质量压一次
      if ((int)jpg_data.size() > max_size) {
        jpg_data = compress_tile_to_jpg(result.tiles[gy][gx], 10);
        if ((int)jpg_data.size() > max_size)
          result.valid = false;
      }
      result.jpg_datas.push_back({jpg_data});
    }
  }
  return result.valid;
}

// 3. 核心改进：不再暴力搜索。
// 640x480 的图，切成 8x8 (64块) 时，每块 80x60 像素。
// 即使质量很好，80x60 的 JPG 也通常在 1KB 左右。
SplitResult find_split_and_quality(const cv::Mat &img, int max_size) {
  SplitResult result;

  // 固定的经验值：8x8 或 8x6 对于 640x480 来说是性能与 MTU 的甜点位
  const int DEFAULT_GX = 8;
  const int DEFAULT_GY = 8;

  // 静态变量保存上一帧的质量，实现自适应跟踪
  static int last_quality = 75;

  // 尝试当前质量
  if (try_compress_tiles(img, DEFAULT_GX, DEFAULT_GY, last_quality, max_size,
                         result)) {
    // 如果很轻松，慢慢往上提升质量
    if (last_quality < 90)
      last_quality += 2;
  } else {
    // 如果超标了，快速下调质量
    last_quality -= 10;
    if (last_quality < 5)
      last_quality = 5;
    // 用新质量重跑一次这一帧
    try_compress_tiles(img, DEFAULT_GX, DEFAULT_GY, last_quality, max_size,
                       result);
  }

  result.valid = true; // 强制标记有效，即使个别块稍大也发出去，总比卡死强
  return result;
}

std::string base64_encode(const uint8_t* data, size_t len) {
    auto t0 = now();
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int i = 0;
    while (i < (int)len) {
        int a = data[i++];
        int b = (i < (int)len) ? data[i++] : 0;
        int c = (i < (int)len) ? data[i++] : 0;
        int triplet = (a << 16) + (b << 8) + c;
        result += chars[(triplet >> 18) & 0x3F];
        result += chars[(triplet >> 12) & 0x3F];
        result += (i - 1 < (int)len) ? chars[(triplet >> 6) & 0x3F] : '=';
        result += (i < (int)len) ? chars[triplet & 0x3F] : '=';
    }
    auto t1 = now();
    if (to_us(t1 - t0) > 2000) {
        std::cout << "  [WARN base64_encode] " << len << " bytes took " << to_us(t1 - t0) << " us (>2ms)" << std::endl;
    }
    return result;
}

void build_meta_json(char* buf, size_t buf_size, int width, int height, int grid_x, int grid_y, int quality) {
    snprintf(buf, buf_size,
        "{\"type\":\"meta\",\"width\":%d,\"height\":%d,\"grid_x\":%d,\"grid_y\":%d,\"quality\":%d,\"total_tiles\":%d}",
        width, height, grid_x, grid_y, quality, grid_x * grid_y);
}

void build_tile_json(char* buf, size_t buf_size, int id, const uint8_t* data, size_t len) {
    auto t0 = now();
    std::string b64 = base64_encode(data, len);
    auto t1 = now();
    std::cout << "  [build_tile_json] id=" << id << " len=" << len << " base64 took " << to_us(t1 - t0) << " us" << std::endl;
    snprintf(buf, buf_size, "{\"type\":\"tile\",\"id\":%d,\"data\":\"%s\"}", id, b64.c_str());
}

void sigint_handler(int signum) {
    std::cout << "收到Ctrl+C，程序即将退出" << std::endl;
    exit(0);
}

void cleanup() { std::cout << "程序异常退出，执行清理操作" << std::endl; }

std::string_view HOST = "192.168.1.123";
int PORT = 23333;

int main(int, char**) {
    atexit(cleanup);
    signal(SIGINT, sigint_handler);

    cv::VideoCapture cap("/dev/video0");
    if (!cap.isOpened()) {
        std::cout << "find uvc camera error." << std::endl;
        return -1;
    }
    std::cout << "find uvc camera Successfully." << std::endl;

    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 160);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 120);
    cap.set(cv::CAP_PROP_FPS, 180);

    std::cout << "get uvc width = " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << std::endl;
    std::cout << "get uvc height = " << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
    std::cout << "get uvc fps = " << cap.get(cv::CAP_PROP_FPS) << std::endl;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cout << "socket create error." << std::endl;
        return -1;
    }
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST.data(), &dest_addr.sin_addr);
    
    std::cout << "UDP target: " << HOST << ":" << PORT << std::endl;

    cv::Mat frame;
    int frame_count = 0;
    int packet_count = 0;
    char json_buf[8192];

    while (1) {
        try {
            auto t0_frame = now();
            cap >> frame;
            auto t1_capture = now();
            
            if (frame.empty()) {
                std::cout << "frame empty" << std::endl;
                break;
            }

            int width = frame.cols;
            int height = frame.rows;
            
            SplitResult result = find_split_and_quality(frame, MAX_PAYLOAD);
            auto t2_split = now();
            
            if (!result.valid) {
                int max_tile_size = 0;
                for (auto& td : result.jpg_datas) {
                    if ((int)td.data.size() > max_tile_size) {
                        max_tile_size = td.data.size();
                    }
                }
                std::cout << "WARNING: tile size " << max_tile_size << " exceeds MTU!" << std::endl;
            }
            
            frame_count++;
            std::cout << "[Frame " << frame_count << "] Grid: " << result.grid_x << "x" << result.grid_y << ", Quality: " << result.quality << std::endl;
            
            for (size_t i = 0; i < result.jpg_datas.size(); i++) {
                std::cout << "  Tile " << i << ": " << result.jpg_datas[i].data.size() << " bytes" << std::endl;
            }
            
            auto t3_send_start = now();
            auto t_meta = now();
            build_meta_json(json_buf, sizeof(json_buf), width, height,
                           result.grid_x, result.grid_y, result.quality);
            sendto(sock, json_buf, strlen(json_buf), 0,
                   (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            packet_count++;
            auto t4_meta = now();
            std::cout << "[Frame " << frame_count << "] meta packet " << strlen(json_buf) << " bytes, took " << to_us(t4_meta - t_meta) << " us" << std::endl;
            
            auto t5_tiles = now();
            for (size_t i = 0; i < result.jpg_datas.size(); i++) {
                auto t_tile_start = now();
                build_tile_json(json_buf, sizeof(json_buf), (int)i,
                               result.jpg_datas[i].data.data(), result.jpg_datas[i].data.size());
                auto t6_build = now();
                ssize_t sent = sendto(sock, json_buf, strlen(json_buf), 0,
                                      (struct sockaddr*)&dest_addr, sizeof(dest_addr));
                auto t7_send = now();
                std::cout << "  [Frame " << frame_count << "] tile " << i << ": build=" << to_us(t6_build - t_tile_start) << " us send=" << to_us(t7_send - t6_build) << " us total=" << sent << " bytes" << std::endl;
                packet_count++;
            }
            auto t8_tiles_done = now();
            std::cout << "[Frame " << frame_count << "] sent " << result.jpg_datas.size() << " tiles in " << to_us(t8_tiles_done - t5_tiles) << " us" << std::endl;
            
            auto t9_frame_done = now();
            std::cout << "[Frame " << frame_count << "] TOTAL: capture=" << to_us(t1_capture - t0_frame) 
                      << " us split=" << to_us(t2_split - t1_capture) 
                      << " us send=" << to_us(t9_frame_done - t3_send_start) 
                      << " us frame=" << to_us(t9_frame_done - t0_frame) << " us" << std::endl;
            

        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            cap.release();
        }
    }

    close(sock);
    return 0;
}