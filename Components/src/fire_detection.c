#include "fire_detection.h"
#include <stdio.h>
#include <string.h>

// 默认火焰检测配置
static FireDetectionConfig fire_config = {
    .h_min = 0,      // 红色调范围开始
    .h_max = 35,     // 橙色/黄色范围结束 (OpenCV格式 0-180)
    .s_min = 80,     // 饱和度最小值 (火焰通常较饱和)
    .v_min = 100,    // 明度最小值 (火焰较亮)
    .min_area = 50,  // 最小火焰区域 50像素
    .flicker_threshold = 3  // 连续3帧变化认为闪烁
};

// 历史记录用于闪烁检测
#define HISTORY_SIZE 10
static uint8_t detection_history[HISTORY_SIZE];
static uint8_t history_index = 0;

/**
 * @brief  初始化火焰检测
 * @param  config: 检测配置,为NULL则使用默认配置
 * @retval 无
 */
void fire_detection_init(FireDetectionConfig* config)
{
    if(config != NULL) {
        fire_config = *config;
    }
    
    // 清空历史记录
    memset(detection_history, 0, sizeof(detection_history));
    history_index = 0;
    
    printf("[Fire Detection] Initialized\r\n");
    printf("  HSV Range: H[%d-%d], S>=%d, V>=%d\r\n", 
           fire_config.h_min, fire_config.h_max, 
           fire_config.s_min, fire_config.v_min);
    printf("  Min Area: %d pixels\r\n", fire_config.min_area);
}

/**
 * @brief  判断是否为火焰像素
 * @param  hsv: HSV颜色值
 * @retval 1-是火焰像素, 0-不是
 * @note   基于HSV颜色空间的火焰颜色特征
 */
uint8_t fire_detection_is_fire_pixel(hsv_t* hsv)
{
    // 火焰颜色特征: 红色到黄色范围, 高饱和度, 高亮度
    if(hsv->h >= fire_config.h_min && hsv->h <= fire_config.h_max &&
       hsv->s >= fire_config.s_min &&
       hsv->v >= fire_config.v_min) {
        return 1;
    }
    return 0;
}

/**
 * @brief  处理单帧图像进行火焰检测（整帧HSV处理）
 * @param  hsv_data: HSV图像数据（外部SRAM）
 * @param  width: 图像宽度
 * @param  height: 图像高度
 * @param  result: 检测结果输出
 * @retval 无
 * @note   遍历整帧HSV数据，统计火焰像素并计算质心
 */
void fire_detection_process_frame(hsv_t* hsv_data, uint16_t width, uint16_t height, FireDetectionResult* result)
{
    uint16_t x, y;
    uint32_t fire_pixel_count = 0;
    uint32_t sum_x = 0, sum_y = 0;
    uint32_t sum_intensity = 0;
    uint8_t fire_detected = 0;
    
    // 初始化结果
    memset(result, 0, sizeof(FireDetectionResult));
    
    // 遍历所有像素，检测火焰颜色
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            hsv_t* hsv = &hsv_data[y * width + x];
            
            if(fire_detection_is_fire_pixel(hsv)) {
                fire_pixel_count++;
                
                // 使用HSV的V值作为火焰强度
                uint16_t intensity = hsv->v;
                
                // 累加加权坐标（强度作为权重）
                sum_x += (uint32_t)x * intensity;
                sum_y += (uint32_t)y * intensity;
                sum_intensity += intensity;
            }
        }
    }
    
    // 判断检测结果
    if(fire_pixel_count >= fire_config.min_area) {
        fire_detected = 1;
        result->fire_detected = 1;
        result->fire_area = fire_pixel_count;
        result->sum_intensity = sum_intensity;
        
        // 计算质心坐标（加权平均）
        if(sum_intensity > 0) {
            result->fire_center_x = (uint16_t)(sum_x / sum_intensity);
            result->fire_center_y = (uint16_t)(sum_y / sum_intensity);
        } else {
            // 如果总强度为0，使用简单平均
            result->fire_center_x = (uint16_t)(sum_x / fire_pixel_count);
            result->fire_center_y = (uint16_t)(sum_y / fire_pixel_count);
        }
        
        // 计算置信度（基于面积占比）
        uint32_t total_pixels = width * height;
        uint8_t area_ratio = (uint8_t)((fire_pixel_count * 100) / total_pixels);
        result->confidence = (area_ratio > 100) ? 100 : area_ratio;
        
        // 确保置信度至少有一定值
        if(result->confidence < 10) {
            result->confidence = 10 + (fire_pixel_count / fire_config.min_area);
            if(result->confidence > 100) result->confidence = 100;
        }
        
        // 基于强度调整置信度（强度越高，置信度越高）
        uint16_t avg_intensity = (uint16_t)(sum_intensity / fire_pixel_count);
        if(avg_intensity > 200) {
            result->confidence = (result->confidence * 110) / 100;
            if(result->confidence > 100) result->confidence = 100;
        }
    }
    
    // 更新历史记录
    fire_detection_update_history(fire_detected);
    
    // 检测闪烁特征
    result->flicker_detected = fire_detection_check_flicker();
    
    // 如果检测到闪烁，增加置信度
    if(result->flicker_detected && result->confidence < 90) {
        result->confidence += 10;
    }
}

/**
 * @brief  更新检测历史记录
 * @param  fire_detected: 当前帧是否检测到火焰
 * @retval 无
 */
void fire_detection_update_history(uint8_t fire_detected)
{
    detection_history[history_index] = fire_detected;
    history_index = (history_index + 1) % HISTORY_SIZE;
}

/**
 * @brief  检测火焰闪烁特征
 * @param  无
 * @retval 1-检测到闪烁, 0-未检测到
 * @note   通过分析历史记录中的变化频率判断闪烁
 */
uint8_t fire_detection_check_flicker(void)
{
    uint8_t i;
    uint8_t change_count = 0;
    uint8_t prev = detection_history[0];
    
    // 统计状态变化次数
    for(i = 1; i < HISTORY_SIZE; i++) {
        if(detection_history[i] != prev) {
            change_count++;
            prev = detection_history[i];
        }
    }
    
    // 变化次数超过阈值认为有闪烁
    if(change_count >= fire_config.flicker_threshold) {
        return 1;
    }
    
    return 0;
}
