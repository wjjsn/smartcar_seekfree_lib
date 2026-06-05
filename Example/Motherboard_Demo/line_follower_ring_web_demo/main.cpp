// line_follower_ring_web_demo
//
// 目标：
//   1. 打开摄像头抓取画面；
//   2. 送入 find_line_lib::calculate_wheel_speeds，库内部会画出
//      紫色 legacy_target 与黄色 folded_target 等调试标注；
//   3. 通过 civetweb 把带这些标注的图像以 MJPEG 推流到网页上。
//
// 与 line_follower_ring 主工程的区别：
//   - 不下放运动控制（不接 PWM / GPIO / 编码器），纯可视化调试。
//   - 输出端口 8089（避免与 web_camera_demo 的 8088 冲突）。

#include <atomic>
#include <chrono>
#include <civetweb.h>
#include <cstring>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <vector>

#include "find_line_lib/calculate_wheel_speeds.h"

namespace {

// 全局运行标志与可被 HTTP 回调消费的最近一帧
std::atomic<bool> g_running{false};
std::atomic<bool> g_camera_ready{false};

// 用 mutex 保护的最新一帧带紫/黄点的调试图
std::mutex g_frame_mutex;
cv::Mat g_latest_frame;

// HTML 页面：嵌一张 /video_feed 的 MJPEG 流图，并附说明
const char *kHtmlPage = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>find_line_lib 可视化</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f0f0f0;
            text-align: center;
        }
        h1 { color: #333; }
        .legend { margin: 12px auto; max-width: 640px; text-align: left; }
        .dot {
            display: inline-block; width: 10px; height: 10px;
            border-radius: 50%; margin-right: 6px; vertical-align: middle;
        }
        .purple { background: #ff00ff; }
        .yellow { background: #ffff00; }
        #videoContainer {
            background: #fff;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            padding: 20px;
            display: inline-block;
        }
        #cameraFeed {
            max-width: 100%;
            width: 480px;
            height: 360px;
            border: 2px solid #333;
            border-radius: 4px;
            background: #000;
        }
        #status {
            margin-top: 10px;
            padding: 10px;
            border-radius: 4px;
        }
        .status-ok { background-color: #d4edda; color: #155724; }
        .status-error { background-color: #f8d7da; color: #721c24; }
    </style>
</head>
<body>
    <h1>find_line_lib 调试图像</h1>
    <div class="legend">
        <div><span class="dot purple"></span>紫色点：legacy_target（最远端点）</div>
        <div><span class="dot yellow"></span>黄色点：folded_target（按路径长度收缩后的目标）</div>
    </div>
    <div id="videoContainer">
        <img id="cameraFeed" src="/video_feed" alt="Camera Feed">
        <div id="status" class="status-ok">Streaming</div>
    </div>
</body>
</html>
)HTML";

// 把一帧 BGR 编码为 JPEG 字节
bool encode_jpeg(const cv::Mat &bgr, std::vector<uchar> &buf) {
  if (bgr.empty())
    return false;
  // 推流到网页时适度缩小，省 CPU 与带宽；保持宽高比例，最大边 480
  cv::Mat small;
  const int max_side = 480;
  int w = bgr.cols, h = bgr.rows;
  if (w > max_side || h > max_side) {
    double scale = std::min(max_side / static_cast<double>(w),
                            max_side / static_cast<double>(h));
    cv::resize(bgr, small, cv::Size(), scale, scale, cv::INTER_AREA);
  } else {
    small = bgr;
  }
  return cv::imencode(".jpg", small, buf, {cv::IMWRITE_JPEG_QUALITY, 80});
}

int handle_request(struct mg_connection *conn) {
  const struct mg_request_info *req_info = mg_get_request_info(conn);
  if (!req_info)
    return 0;

  std::string uri = req_info->local_uri ? req_info->local_uri : "";

  // 根路径 -> HTML 页面
  if (uri == "/" || uri == "") {
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/html; charset=utf-8\r\n"
              "Content-Length: %zu\r\n\r\n",
              std::strlen(kHtmlPage));
    mg_write(conn, kHtmlPage, std::strlen(kHtmlPage));
    return 200;
  }

  // MJPEG 推流
  if (uri == "/video_feed") {
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
              "Cache-Control: no-cache, no-store, must-revalidate\r\n"
              "Pragma: no-cache\r\n"
              "Connection: keep-alive\r\n\r\n");

    while (g_running.load()) {
      cv::Mat frame_copy;
      {
        std::lock_guard<std::mutex> lk(g_frame_mutex);
        if (!g_latest_frame.empty()) {
          frame_copy = g_latest_frame.clone();
        }
      }
      if (frame_copy.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        continue;
      }

      std::vector<uchar> jpeg_buf;
      if (!encode_jpeg(frame_copy, jpeg_buf)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        continue;
      }

      std::string part_header = "--frame\r\n"
                                "Content-Type: image/jpeg\r\n"
                                "Content-Length: " +
                                std::to_string(jpeg_buf.size()) + "\r\n\r\n";

      if (mg_write(conn, part_header.c_str(), part_header.size()) <= 0)
        break;
      if (mg_write(conn, reinterpret_cast<const char *>(jpeg_buf.data()),
                   jpeg_buf.size()) <= 0)
        break;
      if (mg_write(conn, "\r\n", 2) <= 0)
        break;
    }
    return 200;
  }

  // 404
  const char *not_found = "Not Found";
  mg_printf(conn,
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n\r\n",
            std::strlen(not_found));
  mg_write(conn, not_found, std::strlen(not_found));
  return 404;
}

} // namespace

int main() {
  std::cout << "OpenCV version: " << CV_VERSION << std::endl;

  // 打开相机：与 line_follower_ring 一致用 160x120
  cv::VideoCapture cap(0);
  if (!cap.isOpened()) {
    std::cerr << "Error: Cannot open camera" << std::endl;
    return 1;
  }
  cap.set(cv::CAP_PROP_FRAME_WIDTH, 160);
  cap.set(cv::CAP_PROP_FRAME_HEIGHT, 120);
  cap.set(cv::CAP_PROP_FPS, 60);
  std::cout << "Camera: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
            << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << "@"
            << cap.get(cv::CAP_PROP_FPS) << "fps" << std::endl;
  g_camera_ready.store(true);

  // 启动 civetweb
  struct mg_callbacks callbacks;
  std::memset(&callbacks, 0, sizeof(callbacks));
  callbacks.begin_request = handle_request;

  const char *options[] = {
      "document_root",      ".", "listening_ports", "8089",
      "request_timeout_ms", "0", nullptr,
  };

  struct mg_init_data init;
  std::memset(&init, 0, sizeof(init));
  init.callbacks = &callbacks;
  init.configuration_options = options;

  struct mg_error_data error;
  char error_buf[512];
  std::memset(&error, 0, sizeof(error));
  error.text = error_buf;
  error.text_buffer_size = sizeof(error_buf);

  std::cout << "Starting server on http://localhost:8089" << std::endl;
  mg_context *ctx = mg_start2(&init, &error);
  if (!ctx) {
    std::cerr << "Error: Cannot start server";
    if (error.text && error.text_buffer_size > 0) {
      std::cerr << ": " << error.text;
    }
    std::cerr << std::endl;
    cap.release();
    return 1;
  }
  g_running.store(true);
  std::cout << "Server started. Open http://localhost:8089" << std::endl;
  std::cout << "  /            -> HTML 页面" << std::endl;
  std::cout << "  /video_feed  -> MJPEG 推流（带紫/黄点标注）" << std::endl;

  // 主循环：抓帧 -> 调 find_line_lib -> 存到全局
  cv::Mat frame;
  while (g_running.load()) {
    if (!cap.read(frame) || frame.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }

    cv::Mat debug_img;
    auto [left_speed, right_speed] = find_line_lib::calculate_wheel_speeds(
        frame, /*base_speed=*/20.0f, /*max_gain_ratio=*/0.2f, debug_img);

    // 把库吐出来的带紫/黄点的图喂给网页
    if (!debug_img.empty()) {
      std::lock_guard<std::mutex> lk(g_frame_mutex);
      g_latest_frame = debug_img;
    }

    // 控制台简单输出一下速度
    static auto last_print = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (now - last_print > std::chrono::milliseconds(500)) {
      last_print = now;
      std::cout << "L=" << left_speed << " R=" << right_speed
                << "  debug=" << (debug_img.empty() ? 0 : debug_img.cols) << "x"
                << (debug_img.empty() ? 0 : debug_img.rows) << "\r"
                << std::flush;
    }
  }

  g_running.store(false);
  {
    std::lock_guard<std::mutex> lk(g_frame_mutex);
    g_latest_frame.release();
  }
  mg_stop(ctx);
  cap.release();
  std::cout << std::endl << "Server stopped" << std::endl;
  return 0;
}
