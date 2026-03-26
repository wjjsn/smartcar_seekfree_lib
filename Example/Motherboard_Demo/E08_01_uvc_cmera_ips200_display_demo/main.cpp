#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <opencv2/opencv.hpp>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include "zf_common_headfile.h"

#define KEY_0       "/dev/zf_driver_gpio_key_0"

void sigint_handler(int signum)
{
    printf("收到Ctrl+C，程序即将退出\n");
    exit(0);
}

void cleanup()
{
    printf("程序异常退出，执行清理操作\n");
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

    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        perror("open fb0 error");
        return -1;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

    int screen_width = vinfo.xres;
    int screen_height = vinfo.yres;
    int line_length = finfo.line_length;

    printf("screen: %dx%d, line_length: %d, bpp: %d\n", screen_width, screen_height, line_length, vinfo.bits_per_pixel);

    unsigned short *fb_ptr = (unsigned short *)mmap(NULL, finfo.smem_len, PROT_WRITE | PROT_READ, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED) {
        perror("mmap error");
        close(fb_fd);
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

        for (int y = 0; y < screen_height; y++)
        {
            for (int x = 0; x < screen_width; x++)
            {
                int src_y = y * 480 / screen_height;
                int src_x = x * 640 / screen_width;

                cv::Vec3b pixel = frame.at<cv::Vec3b>(src_y, src_x);
                uint8_t r = pixel[2];
                uint8_t g = pixel[1];
                uint8_t b = pixel[0];

                uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                fb_ptr[y * (line_length / 2) + x] = rgb565;
            }
        }

        if(0 == gpio_get_level(KEY_0))
        {
            std::string filename = "out/" + std::to_string(img_count) + ".png";
            cv::imwrite(filename, frame);
            printf("save: %s\n", filename.c_str());
            img_count++;
            while(0 == gpio_get_level(KEY_0));
        }
    }

    munmap(fb_ptr, finfo.smem_len);
    close(fb_fd);
    return 0;
}