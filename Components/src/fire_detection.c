/**
 * @file    fire_detection.c
 * @brief   基于机器视觉的火焰状态检测实现
 * @note    结合 HSV 静态特征提取与历史帧频闪检测实现综合判定。
 */
#include "fire_detection.h"
#include <stdio.h>
#include <string.h>

/* 系统默认视觉算法配置参数 */
static FireDetectionConfig fire_config = {
    .h_min = 0,      /* 红色调范围下界 */
    .h_max = 35,     /* 橙色/黄色范围上界 (OpenCV格式 0-180) */
    .s_min = 80,     /* 火焰具有高饱和度特征的下限 */
    .v_min = 100,    /* 发光体明度下限 */
    .min_area = 50,  /* 过滤噪点的最小面积阈值 */
    .flicker_threshold = 3  /* 视为频闪特征所需的状态跳变次数 */
};

/* 闪烁检测缓存与控制游标 */
#define HISTORY_SIZE 10
static uint8_t detection_history[HISTORY_SIZE];
static uint8_t history_index = 0;

/**
 * @brief  初始化火焰检测算法参数及缓存状态
 * @param  config 检测配置文件指针，传入 NULL 则使用系统默认值
 */
void fire_detection_init(FireDetectionConfig* config)
{
    if(config != NULL) {
        fire_config = *config;
    }
    
    memset(detection_history, 0, sizeof(detection_history));
    history_index = 0;
    
    printf("[Fire Detection] Initialized\r\n");
    printf("  HSV Range: H[%d-%d], S>=%d, V>=%d\r\n", 
           fire_config.h_min, fire_config.h_max, 
           fire_config.s_min, fire_config.v_min);
    printf("  Min Area: %d pixels\r\n", fire_config.min_area);
}

/**
 * @brief  颜色空间像素级特征筛选
 * @param  hsv 当前像素的颜色向量
 * @return uint8_t 1:命中特征, 0:非目标像素
 */
uint8_t fire_detection_is_fire_pixel(hsv_t* hsv)
{
    if(hsv->h >= fire_config.h_min && hsv->h <= fire_config.h_max &&
       hsv->s >= fire_config.s_min &&
       hsv->v >= fire_config.v_min) {
        return 1;
    }
    return 0;
}

/**
 * @brief  图像帧全域特征提取及综合计算
 * @param  hsv_data HSV空间图像数据缓冲区
 * @param  width    图像宽度
 * @param  height   图像高度
 * @param  result   检测结果返回结构
 * @note   计算包含像素统计、基于明度权重的质心定位、以及综合置信度赋值。
 */
void fire_detection_process_frame(hsv_t* hsv_data, uint16_t width, uint16_t height, FireDetectionResult* result)
{
    uint16_t x, y;
    uint32_t fire_pixel_count = 0;
    uint32_t sum_x = 0, sum_y = 0;
    uint32_t sum_intensity = 0;
    uint8_t  fire_detected = 0;
    
    memset(result, 0, sizeof(FireDetectionResult));
    
    /* 遍历像素阵列，统计目标特征参数 */
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            hsv_t* hsv = &hsv_data[y * width + x];
            
            if(fire_detection_is_fire_pixel(hsv)) {
                fire_pixel_count++;
                
                uint16_t intensity = hsv->v;
                
                sum_x += (uint32_t)x * intensity;
                sum_y += (uint32_t)y * intensity;
                sum_intensity += intensity;
            }
        }
    }
    
    /* 面积阈值过滤及综合置信度计算 */
    if(fire_pixel_count >= fire_config.min_area) {
        fire_detected = 1;
        result->fire_detected = 1;
        result->fire_area = fire_pixel_count;
        result->sum_intensity = sum_intensity;
        
        /* 采用明度权重进行质心定位 */
        if(sum_intensity > 0) {
            result->fire_center_x = (uint16_t)(sum_x / sum_intensity);
            result->fire_center_y = (uint16_t)(sum_y / sum_intensity);
        } else {
            result->fire_center_x = (uint16_t)(sum_x / fire_pixel_count);
            result->fire_center_y = (uint16_t)(sum_y / fire_pixel_count);
        }
        
        /* 基础置信度评估 (依据有效像素占全图比例) */
        uint32_t total_pixels = width * height;
        uint8_t area_ratio = (uint8_t)((fire_pixel_count * 100) / total_pixels);
        result->confidence = (area_ratio > 100) ? 100 : area_ratio;
        
        if(result->confidence < 10) {
            result->confidence = 10 + (fire_pixel_count / fire_config.min_area);
            if(result->confidence > 100) result->confidence = 100;
        }
        
        /* 置信度权重强化 (对于高发光特性的物体调高权重) */
        uint16_t avg_intensity = (uint16_t)(sum_intensity / fire_pixel_count);
        if(avg_intensity > 200) {
            result->confidence = (result->confidence * 110) / 100;
            if(result->confidence > 100) result->confidence = 100;
        }
    }
    
    /* 更新频闪分析窗口状态 */
    fire_detection_update_history(fire_detected);
    
    /* 频闪特征确认及置信度补偿 */
    result->flicker_detected = fire_detection_check_flicker();
    
    if(result->flicker_detected && result->confidence < 90) {
        result->confidence += 10;
    }
}

/**
 * @brief  缓存单帧状态特征，驱动频闪分析状态机
 * @param  fire_detected 单帧检测结论
 */
void fire_detection_update_history(uint8_t fire_detected)
{
    detection_history[history_index] = fire_detected;
    history_index = (history_index + 1) % HISTORY_SIZE;
}

/**
 * @brief  分析历史缓存窗口中的高频状态跳变
 * @return uint8_t 1:存在高频跳变(频闪特征), 0:图像静态
 */
uint8_t fire_detection_check_flicker(void)
{
    uint8_t i;
    uint8_t change_count = 0;
    uint8_t prev = detection_history[0];
    
    for(i = 1; i < HISTORY_SIZE; i++) {
        if(detection_history[i] != prev) {
            change_count++;
            prev = detection_history[i];
        }
    }
    
    if(change_count >= fire_config.flicker_threshold) {
        return 1;
    }
    
    return 0;
}
