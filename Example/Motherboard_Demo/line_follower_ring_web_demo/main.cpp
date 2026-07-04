// line_follower_ring_web_demo
//
// 目标：
//   提供一个类似 OpenCV 的 `imshow(窗口名, 图像)` 接口，
//   让网页端能够根据不同的"窗口名"自动创建并刷新不同的独立窗口。
//
// 实现已迁移至 find_line_lib 库 (web_imshow.h / web_imshow.cpp)，
// 本文件仅负责摄像头采集 + 调用库内算法 + 调用库内 imshow。

#include <atomic>
#include <chrono>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>

#include "find_line_lib/calculate_wheel_speeds.h"
#include "find_line_lib/ring.h"
#include "find_line_lib/web_imshow.h"

inline std::atomic<bool> g_running{false};

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
    
    double actual_w = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    double actual_h = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << "[INFO] 摄像头初始化成功。当前硬件输出分辨率: " << actual_w << "x" << actual_h << std::endl;

    // 3. 启动 Web 服务器（RAII：离开作用域时自动停止）
    find_line_lib::WebImShow server("8089");
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

        // 4. 安全防御：如果摄像头固件拒绝了 160x120 的请求，手动缩放
        if (frame_raw.cols != 160 || frame_raw.rows != 120) {
            cv::resize(frame_raw, frame, cv::Size(160, 120), 0, 0, cv::INTER_AREA);
        } else {
            frame = frame_raw;
        }

        // ─── 算法基本操作 ───
        cv::Mat color_debug_frame = frame.clone(); 
        cv::Mat bin_frame = ring_detector.check_ring(frame);
        if (bin_frame.empty()) continue;

        cv::Mat result_view = bin_frame.clone(); 

        cv::copyMakeBorder(bin_frame, bin_frame, 1, 0, 1, 1, cv::BORDER_CONSTANT, cv::Scalar(0));
        auto result = tools.find_center_line(bin_frame, -1, -1);
        for (const auto &p : result) {
            cv::circle(color_debug_frame, cv::Point(p.x, p.y), 1, cv::Scalar(0, 255, 0), cv::FILLED);
        }
        
        // 只要你更换窗口名，网页上就会自动多弹出一个独立的窗口！
        server.imshow("1. Result", color_debug_frame);
        server.imshow("2. Debug", result_view);
    }

    g_running.store(false);
    cap.release();
    // server 离开作用域时自动调用析构停止服务器
    return 0;
}