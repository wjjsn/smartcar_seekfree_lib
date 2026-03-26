#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) return -1;

    std::vector<std::pair<int, int>> res = {{640,480}, {1280,720}, {1920,1080}};

    for (auto &r : res) {
        cap.set(cv::CAP_PROP_FRAME_WIDTH, r.first);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, r.second);
        
        double w = cap.get(cv::CAP_PROP_FRAME_WIDTH);
        double h = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
        double fps = cap.get(cv::CAP_PROP_FPS);
        
        std::cout << "尝试: " << r.first << "x" << r.second 
                  << " => 实际: " << w << "x" << h 
                  << " @ " << fps << "fps" << std::endl;
    }
    return 0;
}