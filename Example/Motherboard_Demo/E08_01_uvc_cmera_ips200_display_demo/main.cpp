#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <opencv2/opencv.hpp>
#include "zf_common_headfile.h"
#include "framebuffer.h"

#define KEY_0       "/dev/zf_driver_gpio_key_0"

void sigint_handler(int signum)
{
    printf("收到Ctrl+C，程序即将退出\n");
    framebuffer_cleanup();
    exit(0);
}

void cleanup()
{
    printf("程序异常退出，执行清理操作\n");
    framebuffer_cleanup();
}

int main(int, char**)
{
    atexit(cleanup);
    signal(SIGINT, sigint_handler);

    cv::VideoCapture cap("/dev/video0");
    if(!cap.isOpened())
    {
        printf("find uvc camera error.\n");
        return -1;
    }
    printf("find uvc camera Successfully.\n");

    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 120);

    printf("get uvc width = %f\n", cap.get(cv::CAP_PROP_FRAME_WIDTH));
    printf("get uvc height = %f\n", cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    printf("get uvc fps = %f\n", cap.get(cv::CAP_PROP_FPS));

    if (framebuffer_init() < 0) {
        return -1;
    }

    cv::Mat frame;
    int img_count = 0;

    while(1)
    {
        cap >> frame;
        if(frame.empty())
        {
            printf("frame empty\n");
            exit(0);
        }

        framebuffer_update(frame.data, frame.cols, frame.rows);

        if(0 == gpio_get_level(KEY_0))
        {
            std::string filename = "out/" + std::to_string(img_count) + ".png";
            cv::imwrite(filename, frame);
            printf("save: %s\n", filename.c_str());
            img_count++;
            while(0 == gpio_get_level(KEY_0));
        }
    }

    framebuffer_cleanup();
    return 0;
}
