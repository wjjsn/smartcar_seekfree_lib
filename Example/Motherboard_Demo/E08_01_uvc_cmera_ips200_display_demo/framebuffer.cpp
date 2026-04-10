#include "framebuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>

static int fb_fd = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static unsigned short *fb_ptr = NULL;
static int screen_width = 0;
static int screen_height = 0;
static int line_length = 0;

int framebuffer_init(void)
{
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        perror("open fb0 error");
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("FBIOGET_VSCREENINFO error");
        close(fb_fd);
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("FBIOGET_FSCREENINFO error");
        close(fb_fd);
        return -1;
    }

    screen_width = vinfo.xres;
    screen_height = vinfo.yres;
    line_length = finfo.line_length;

    printf("screen: %dx%d, line_length: %d, bpp: %d\n", screen_width, screen_height, line_length, vinfo.bits_per_pixel);

    fb_ptr = (unsigned short *)mmap(NULL, finfo.smem_len, PROT_WRITE | PROT_READ, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED) {
        perror("mmap error");
        close(fb_fd);
        return -1;
    }

    return 0;
}

void framebuffer_update(const uint8_t* image_data, int img_width, int img_height)
{
    if (fb_ptr == NULL) {
        return;
    }

    for (int y = 0; y < screen_height; y++) {
        for (int x = 0; x < screen_width; x++) {
            int src_y = y * img_height / screen_height;
            int src_x = x * img_width / screen_width;

            int index = (src_y * img_width + src_x) * 3;
            uint8_t r = image_data[index + 2];
            uint8_t g = image_data[index + 1];
            uint8_t b = image_data[index + 0];

            uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            fb_ptr[y * (line_length / 2) + x] = rgb565;
        }
    }
}

void framebuffer_cleanup(void)
{
    if (fb_ptr != NULL) {
        munmap(fb_ptr, finfo.smem_len);
        fb_ptr = NULL;
    }
    if (fb_fd >= 0) {
        close(fb_fd);
        fb_fd = -1;
    }
}
