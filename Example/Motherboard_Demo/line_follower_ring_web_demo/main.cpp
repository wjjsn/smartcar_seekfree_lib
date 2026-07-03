// line_follower_ring_web_demo
//
// 目标：
//   提供一个类似 OpenCV 的 `imshow(窗口名, 图像)` 接口，
//   让网页端能够根据不同的“窗口名”自动创建并刷新不同的独立窗口。

#include <atomic>
#include <chrono>
#include <civetweb.h>
#include <cstring>
#include <iostream>
#include <mutex>
#include <map>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <vector>

#include "find_line_lib/calculate_wheel_speeds.h"
#include "find_line_lib/ring.h"

inline std::atomic<bool> g_running{false};

namespace find_line_lib {

// 全局窗口管家变量
inline std::mutex g_window_mutex;
inline std::map<std::string, cv::Mat> g_virtual_windows;

/**
 * @brief 自定义虚拟 Web Imshow 接口
 * 用法与 cv::imshow 完全一致，不同的 winname 会在网页端自动弹窗渲染
 */
inline void imshow(const std::string& winname, cv::InputArray mat) {
    if (mat.empty()) return;
    cv::Mat frame = mat.getMat().clone();
    
    // 自动将图像统一缩放到前端舒适的 400x300 分辨率
    if (frame.cols != 400 || frame.rows != 300) {
        cv::resize(frame, frame, cv::Size(400, 300));
    }
    // 自动将灰度/二值图（如 result_img）转为 BGR，确保 hconcat 拼接时通道一致
    if (frame.channels() == 1) {
        cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR);
    }

    std::lock_guard<std::mutex> lk(g_window_mutex);
    g_virtual_windows[winname] = frame; // 写入或覆盖该命名窗口
}
}


// 它会实时向 C++ 索要当前拼起来的大图，并利用 JavaScript 根据窗口名自动切分、创建 HTML 容器
const char *kHtmlPage = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <title>Web Imshow 动态工作台</title>
    <style>
        body { font-family: 'Segoe UI', sans-serif; margin: 0; padding: 20px; background: #eaeaea; text-align: center; }
        h1 { color: #333; margin-bottom: 20px; }
        .window-container { display: flex; justify-content: center; gap: 20px; max-width: 100%; flex-wrap: wrap; margin: 0 auto; }
        .display-window { background: #fff; border-radius: 8px; box-shadow: 0 4px 15px rgba(0,0,0,0.12); padding: 12px; }
        .window-title { font-size: 13px; font-weight: bold; color: #444; margin-bottom: 8px; text-align: left; background: #f0f0f0; padding: 4px 8px; border-radius: 3px; }
        .crop-viewport { width: 400px; height: 300px; overflow: hidden; position: relative; border: 2px solid #222; border-radius: 4px; background: #000; }
        .mjpeg-stream { position: absolute; top: 0; left: 0; height: 300px; max-width: none; }
    </style>
</head>
<body>
    <h1>OpenCV `imshow` 网页控制台</h1>
    <div class="window-container" id="workspace"></div>

    <script>
        // 动态监听 C++ 端的窗口配置
        async function syncWindows() {
            try {
                let res = await fetch('/window_layout');
                let config = await res.json(); // 拿到格式如: { total: 3, windows: ["Live", "Result", "Gray"] }
                let container = document.getElementById('workspace');
                
                // 检查当前的 DOM 窗口数量和名字是否需要更新
                let currentWins = Array.from(container.querySelectorAll('.window-title')).map(el => el.textContent);
                let configWins = config.windows;
                
                if (JSON.stringify(currentWins) !== JSON.stringify(configWins)) {
                    container.innerHTML = ''; // 发现窗口数量或名字变了，重新生成布局
                    configWins.forEach((winName, index) => {
                        let winHtml = `
                            <div class="display-window">
                                <div class="window-title">${winName}</div>
                                <div class="crop-viewport">
                                    <img class="mjpeg-stream" src="/video_feed" style="width: ${config.total * 400}px; left: -${index * 400}px;">
                                </div>
                            </div>`;
                        container.insertAdjacentHTML('beforeend', winHtml);
                    });
                }
            } catch (e) { console.error("同步窗口失败", e); }
        }
        
        // 每秒检查一次有没有新调用 imshow() 创建新窗口
        setInterval(syncWindows, 1000);
        syncWindows();
    </script>
</body>
</html>
)HTML";

int handle_request(struct mg_connection *conn) {
  const struct mg_request_info *req_info = mg_get_request_info(conn);
  if (!req_info) return 0;
  std::string uri = req_info->local_uri ? req_info->local_uri : "";

  // 1. 主页面
  if (uri == "/" || uri == "") {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\n\r\n", std::strlen(kHtmlPage));
    mg_write(conn, kHtmlPage, std::strlen(kHtmlPage));
    return 200;
  }

  // 路由 1: 实时输出窗口布局元数据
    if (uri == "/window_layout") {
        std::string json = "{";
        {
            std::lock_guard<std::mutex> lk(find_line_lib::g_window_mutex);
            json += "\"total\":" + std::to_string(find_line_lib::g_virtual_windows.size()) + ",\"windows\":[";
            for (auto it = find_line_lib::g_virtual_windows.begin(); it != find_line_lib::g_virtual_windows.end(); ++it) {
                json += "\"" + it->first + "\"";
                if (std::next(it) != find_line_lib::g_virtual_windows.end()) json += ",";
            }
            json += "]}";
        }
        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", json.size());
        mg_write(conn, json.c_str(), json.size());
        return 200;
    }

    // 路由 2: 多路拼装图像视频流
    if (uri == "/video_feed") {
        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\n\r\n");
        while (g_running.load()) {
            cv::Mat combined_frame;
            {
                std::lock_guard<std::mutex> lk(find_line_lib::g_window_mutex);
                if (!find_line_lib::g_virtual_windows.empty()) {
                    std::vector<cv::Mat> views;
                    for (const auto& [name, img] : find_line_lib::g_virtual_windows) {
                        views.push_back(img);
                    }
                    cv::hconcat(views, combined_frame); 
                }
            }
            
            if (combined_frame.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                continue;
            }

            std::vector<uchar> jpeg_buf;
            cv::imencode(".jpg", combined_frame, jpeg_buf, {cv::IMWRITE_JPEG_QUALITY, 75});
            
            std::string part_header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + std::to_string(jpeg_buf.size()) + "\r\n\r\n";
            if (mg_write(conn, part_header.c_str(), part_header.size()) <= 0) break;
            if (mg_write(conn, reinterpret_cast<const char *>(jpeg_buf.data()), jpeg_buf.size()) <= 0) break;
            if (mg_write(conn, "\r\n", 2) <= 0) break;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(25)); // 控制推流帧率在 ~40FPS
        }
        return 200;
    }
    return 404;
}
// namespace

int main() {
    std::cout << "[INFO] 正在尝试通过 V4L2 后端打开摄像头 0..." << std::endl;
    
    // 1. 显式指定 CAP_V4L2 增强在 Linux/Buildroot 上的兼容性
    cv::VideoCapture cap(0, cv::CAP_V4L2);
    if (!cap.isOpened()) {
        std::cerr << "[ERROR] 错误：无法打开摄像头！请检查 /dev/video0 是否存在或权限是否正确。" << std::endl;
        return 1;
    }

    // 2. 尝试设置低分辨率以节省嵌入式 CPU
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 160);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 120);
    
    // 打印实际获取到的硬件分辨率
    double actual_w = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    double actual_h = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << "[INFO] 摄像头初始化成功。当前硬件输出分辨率: " << actual_w << "x" << actual_h << std::endl;

    // 3. 启动 Web 服务器
    struct mg_callbacks callbacks;
    std::memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = handle_request;
    const char *options[] = { "document_root", ".", "listening_ports", "8089", "request_timeout_ms", "0", nullptr };
    struct mg_init_data init;
    std::memset(&init, 0, sizeof(init));
    init.callbacks = &callbacks;
    init.configuration_options = options;
    mg_context *ctx = mg_start2(&init, nullptr);
    g_running.store(true);

    cv::Mat frame_raw, frame;
    static find_line_lib::tools tools;
    static find_line_lib::ring ring_detector;

    std::cout << "[INFO] 工作台已就绪，请在浏览器中打开 http://板子IP:8089" << std::endl;

    while (g_running.load()) {
        if (!cap.read(frame_raw) || frame_raw.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // 4. 安全防御：如果摄像头固件拒绝了 160x120 的请求，强行输出大图，我们就在这里手动缩放
        if (frame_raw.cols != 160 || frame_raw.rows != 120) {
            cv::resize(frame_raw, frame, cv::Size(160, 120), 0, 0, cv::INTER_AREA);
        } else {
            frame = frame_raw; // 分辨率正确，直接赋值（共享特征，零拷贝）
        }

        // ─── 算法基本操作 ───
        cv::Mat color_debug_frame = frame.clone(); 
        cv::Mat bin_frame = ring_detector.check_ring(frame, &color_debug_frame);
        if (bin_frame.empty()) continue;

        cv::Mat result_view = bin_frame.clone(); 

        cv::copyMakeBorder(bin_frame, bin_frame, 1, 0, 1, 1, cv::BORDER_CONSTANT, cv::Scalar(0));
        auto result = tools.find_center_line(bin_frame, -1, -1);
        for (const auto &p : result) {
            cv::circle(color_debug_frame, cv::Point(p.x, p.y), 1, cv::Scalar(0, 255, 0), cv::FILLED);
        }
        
        // 只要你更换窗口名，网页上就会自动多弹出一个独立的窗口！
        find_line_lib::imshow("1. Result", color_debug_frame);  // 丢给窗口 1
        find_line_lib::imshow("2. Debug", result_view);         // 丢给窗口 2
    }

    
    g_running.store(false);
    mg_stop(ctx);
    cap.release();
    return 0;
}