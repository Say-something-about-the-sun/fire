#ifndef __FIRE_DETECTION_H
#define __FIRE_DETECTION_H

#include "sys.h"
#include "image_preprocess.h"

// 火焰检测配置
typedef struct {
    uint8_t h_min;      // HSV色调最小值
    uint8_t h_max;      // HSV色调最大值
    uint8_t s_min;      // 饱和度最小值
    uint8_t v_min;      // 明度最小值
    uint16_t min_area;  // 最小火焰区域面积
    uint8_t flicker_threshold;  // 闪烁检测阈值
} FireDetectionConfig;

// 火焰检测结果
typedef struct {
    uint8_t fire_detected;      // 是否检测到火焰
    uint8_t confidence;         // 置信度 (0-100)
    uint16_t fire_area;         // 火焰区域面积
    uint16_t fire_center_x;     // 火焰质心X坐标
    uint16_t fire_center_y;     // 火焰质心Y坐标
    uint8_t flicker_detected;   // 是否检测到闪烁
    uint32_t sum_intensity;     // 火焰总强度（用于质心计算）
} FireDetectionResult;

// 函数声明
void fire_detection_init(FireDetectionConfig* config);
void fire_detection_process_frame(hsv_t* hsv_data, uint16_t width, uint16_t height, FireDetectionResult* result);
uint8_t fire_detection_is_fire_pixel(hsv_t* hsv);
void fire_detection_update_history(uint8_t fire_detected);
uint8_t fire_detection_check_flicker(void);

#endif /* __FIRE_DETECTION_H */
