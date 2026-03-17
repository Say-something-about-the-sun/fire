#ifndef __IMAGE_PREPROCESS_H
#define __IMAGE_PREPROCESS_H

#include "sys.h"

// HSV颜色空间结构
typedef struct {
    uint8_t h;  // 色调 (0-180, OpenCV格式)
    uint8_t s;  // 饱和度 (0-255)
    uint8_t v;  // 明度 (0-255)
} hsv_t;

// 函数声明
void image_preprocess_rgb_to_hsv(uint16_t rgb565, hsv_t* hsv);
void image_preprocess_row_rgb_to_hsv(uint16_t* rgb_row, hsv_t* hsv_row, uint16_t width);
void image_preprocess_frame_rgb_to_hsv(uint16_t* rgb_data, hsv_t* hsv_data, uint16_t width, uint16_t height);
void image_preprocess_median_filter_3x3(uint16_t* src, uint16_t* dst, uint16_t width, uint16_t height);
void image_preprocess_brightness_normalize(uint16_t* row, uint16_t width);

#endif /* __IMAGE_PREPROCESS_H */
