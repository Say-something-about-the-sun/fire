/**
 * @file    fire_detection.h
 * @brief   基于 HSV 颜色空间的机器视觉火焰检测算法接口
 */
#ifndef __FIRE_DETECTION_H
#define __FIRE_DETECTION_H

#include "sys.h"
#include "image_preprocess.h"

/* 火焰颜色与形态学判定阈值配置 */
typedef struct {
    uint8_t  h_min;             /* HSV色调最小值 */
    uint8_t  h_max;             /* HSV色调最大值 */
    uint8_t  s_min;             /* 饱和度最小值 */
    uint8_t  v_min;             /* 明度最小值 */
    uint16_t min_area;          /* 构成火焰特征的最小像素连通面积 */
    uint8_t  flicker_threshold; /* 构成频闪特征的状态跳变次数阈值 */
} FireDetectionConfig;

/* 视觉检测综合评估结果 */
typedef struct {
    uint8_t  fire_detected;     /* 像素判定是否命中火焰特征 */
    uint8_t  confidence;        /* 算法评估置信度 (0-100) */
    uint16_t fire_area;         /* 命中火焰特征的像素面积 */
    uint16_t fire_center_x;     /* 火焰质心区域的X坐标 */
    uint16_t fire_center_y;     /* 火焰质心区域的Y坐标 */
    uint8_t  flicker_detected;  /* 频闪特征命中状态 */
    uint32_t sum_intensity;     /* 区域像素总体明度权重 */
} FireDetectionResult;

/* 对外功能函数声明 */
void    fire_detection_init(FireDetectionConfig* config);
void    fire_detection_process_frame(hsv_t* hsv_data, uint16_t width, uint16_t height, FireDetectionResult* result);
uint8_t fire_detection_is_fire_pixel(hsv_t* hsv);
void    fire_detection_update_history(uint8_t fire_detected);
uint8_t fire_detection_check_flicker(void);

#endif /* __FIRE_DETECTION_H */
