#include <chrono>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>
#include <thread>
using namespace std::chrono_literals;

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) return -1;

    std::vector<std::pair<int, int>> res = {
        {160, 120}, {320, 240}, {640, 480}, {1280, 720}, {1920, 1080}};
    std::cout << std::fixed << std::setprecision(1);
    for (auto &r : res) {
        cap.set(cv::CAP_PROP_FRAME_WIDTH, r.first);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, r.second);
        cap.set(cv::CAP_PROP_FPS, 180);

        double w = cap.get(cv::CAP_PROP_FRAME_WIDTH);
        double h = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
        double fps = cap.get(cv::CAP_PROP_FPS);
        
        std::cout << "尝试: " << r.first << "x" << r.second 
                  << " => 实际: " << w << "x" << h 
                  << " @ " << fps << "fps" << std::endl;

        cv::Mat frame;
        auto start = std::chrono::steady_clock::now();
        auto frame_count = 0;
        while (frame_count < 1000) {
          auto t0 = std::chrono::steady_clock::now();
          cap >> frame;
          frame_count++;
          auto t1 = std::chrono::steady_clock::now();
          auto duration =
              std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

          auto elapsed_seconds =
              std::chrono::duration_cast<std::chrono::seconds>(t1 - start)
                  .count();
          double average_fps =
              (elapsed_seconds > 0)
                  ? frame_count / static_cast<double>(elapsed_seconds)
                  : 0.0;

          std::cout << "Capture time: " << duration.count() << " ms"
                    << ", Total frames: " << frame_count
                    << ", Average FPS: " << average_fps << '\r';

          if (frame.empty())
            break;
        std::this_thread::sleep_for(1ms);
        start+= 1ms;
        }
        std::cout << '\n';
    }

    return 0;
}