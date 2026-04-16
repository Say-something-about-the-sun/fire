#include "image_preprocess.h"
#include <string.h>

/**
 * @brief  RGB565转HSV (简化快速版本)
 * @param  rgb565: RGB565像素值
 * @param  hsv: HSV输出指针
 * @retval 无
 * @note   H范围0-180(匹配OpenCV), S/V范围0-255
 */
void image_preprocess_rgb_to_hsv(uint16_t rgb565, hsv_t* hsv)
{
    uint8_t r, g, b;
    uint8_t max, min, delta;
    
    // RGB565转RGB888
    r = ((rgb565 >> 11) & 0x1F) << 3;
    g = ((rgb565 >> 5) & 0x3F) << 2;
    b = (rgb565 & 0x1F) << 3;
    
    // 找最大最小值
    max = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    min = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    delta = max - min;
    
    // 计算V (0-255)
    hsv->v = max;
    
    // 计算S (0-255)
    if(max == 0) {
        hsv->s = 0;
    } else {
        hsv->s = (uint8_t)((delta * 255) / max);
    }
    
    // 计算H (0-180, 与OpenCV一致)
    if(delta == 0) {
        hsv->h = 0;
    } else {
        if(max == r) {
            hsv->h = (uint8_t)(((g - b) * 30) / delta);  // 30 = 180/6
            if(g < b) hsv->h += 180;
        } else if(max == g) {
            hsv->h = (uint8_t)(((b - r) * 30) / delta + 60);
        } else {
            hsv->h = (uint8_t)(((r - g) * 30) / delta + 120);
        }
    }
}

/**
 * @brief  RGB行转HSV行
 * @param  rgb_row: RGB565行数据
 * @param  hsv_row: HSV行数据
 * @param  width: 图像宽度
 * @retval 无
 */
void image_preprocess_row_rgb_to_hsv(uint16_t* rgb_row, hsv_t* hsv_row, uint16_t width)
{
    uint16_t i;
    for(i = 0; i < width; i++) {
        image_preprocess_rgb_to_hsv(rgb_row[i], &hsv_row[i]);
    }
}

/**
 * @brief  RGB整帧转HSV整帧
 * @param  rgb_data: RGB565整帧数据
 * @param  hsv_data: HSV整帧数据
 * @param  width: 图像宽度
 * @param  height: 图像高度
 * @retval 无
 * @note   处理整帧图像，适用于外部SRAM中的大缓冲区
 */
void image_preprocess_frame_rgb_to_hsv(uint16_t* rgb_data, hsv_t* hsv_data, uint16_t width, uint16_t height)
{
    uint16_t x, y;
    
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            uint16_t rgb = rgb_data[y * width + x];
            image_preprocess_rgb_to_hsv(rgb, &hsv_data[y * width + x]);
        }
    }
}

/**
 * @brief  3x3中值滤波 (简单快速版本)
 * @param  src: 源图像数据
 * @param  dst: 目标图像数据
 * @param  width: 图像宽度
 * @param  height: 图像高度
 * @retval 无
 * @note   只处理内部像素,边界直接复制
 */
void image_preprocess_median_filter_3x3(uint16_t* src, uint16_t* dst, uint16_t width, uint16_t height)
{
    int16_t x, y;
    uint16_t window[9];
    
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            // 边界直接复制
            if(y == 0 || y == height - 1 || x == 0 || x == width - 1) {
                dst[y * width + x] = src[y * width + x];
                continue;
            }
            
            // 收集3x3窗口
            window[0] = src[(y-1) * width + (x-1)];
            window[1] = src[(y-1) * width + x];
            window[2] = src[(y-1) * width + (x+1)];
            window[3] = src[y * width + (x-1)];
            window[4] = src[y * width + x];
            window[5] = src[y * width + (x+1)];
            window[6] = src[(y+1) * width + (x-1)];
            window[7] = src[(y+1) * width + x];
            window[8] = src[(y+1) * width + (x+1)];
            
            // 简单选择排序找中值(第5个)
            uint8_t i, j;
            for(i = 0; i < 5; i++) {
                uint8_t min_idx = i;
                for(j = i + 1; j < 9; j++) {
                    if(window[j] < window[min_idx]) {
                        min_idx = j;
                    }
                }
                uint16_t temp = window[i];
                window[i] = window[min_idx];
                window[min_idx] = temp;
            }
            
            dst[y * width + x] = window[4];  // 中值
        }
    }
}

/**
 * @brief  亮度归一化 (单行)
 * @param  row: RGB565行数据
 * @param  width: 图像宽度
 * @retval 无
 * @note   将图像亮度调整到标准范围,减少光照影响
 */
void image_preprocess_brightness_normalize(uint16_t* row, uint16_t width)
{
    uint16_t i;
    uint32_t sum_v = 0;
    uint8_t avg_v;
    
    // 计算平均亮度
    for(i = 0; i < width; i++) {
        uint16_t rgb = row[i];
        uint8_t r = ((rgb >> 11) & 0x1F) << 3;
        uint8_t g = ((rgb >> 5) & 0x3F) << 2;
        uint8_t b = (rgb & 0x1F) << 3;
        // 快速亮度估算: (R + 2*G + B) / 4
        sum_v += (r + (g << 1) + b) >> 2;
    }
    
    avg_v = (uint8_t)(sum_v / width);
    
    // 如果亮度偏离标准值(128),进行线性调整
    if(avg_v != 0 && avg_v != 128) {
        uint16_t scale = (128 * 256) / avg_v;  // 定点数缩放因子
        
        for(i = 0; i < width; i++) {
            uint16_t rgb = row[i];
            uint8_t r = ((rgb >> 11) & 0x1F);
            uint8_t g = ((rgb >> 5) & 0x3F);
            uint8_t b = (rgb & 0x1F);
            
            // 调整RGB分量
            r = (r * scale) >> 8;
            g = (g * scale) >> 8;
            b = (b * scale) >> 8;
            
            // 限幅
            if(r > 31) r = 31;
            if(g > 63) g = 63;
            if(b > 31) b = 31;
            
            row[i] = (r << 11) | (g << 5) | b;
        }
    }
}

