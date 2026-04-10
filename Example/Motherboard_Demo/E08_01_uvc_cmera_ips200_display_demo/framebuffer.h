#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <cstdint>

int framebuffer_init(void);

void framebuffer_update(const uint8_t* image_data, int img_width, int img_height);

void framebuffer_cleanup(void);

#endif
