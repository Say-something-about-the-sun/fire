/**
 * @file    image_preprocess.h
 * @brief   图像空间转换与降噪滤波处理接口
 */
#ifndef __IMAGE_PREPROCESS_H
#define __IMAGE_PREPROCESS_H

#include "sys.h"

/* HSV色彩空间抽象数据结构 */
typedef struct {
    uint8_t h;  /* 色调通道 (0-180, 符合标准OpenCV约定规范) */
    uint8_t s;  /* 饱和度通道 (0-255) */
    uint8_t v;  /* 明度通道 (0-255) */
} hsv_t;

/* 数据转换及空间滤波功能声明 */
void image_preprocess_rgb_to_hsv(uint16_t rgb565, hsv_t* hsv);
void image_preprocess_row_rgb_to_hsv(uint16_t* rgb_row, hsv_t* hsv_row, uint16_t width);
void image_preprocess_frame_rgb_to_hsv(uint16_t* rgb_data, hsv_t* hsv_data, uint16_t width, uint16_t height);
void image_preprocess_median_filter_3x3(uint16_t* src, uint16_t* dst, uint16_t width, uint16_t height);
void image_preprocess_brightness_normalize(uint16_t* row, uint16_t width);

#endif /* __IMAGE_PREPROCESS_H */
