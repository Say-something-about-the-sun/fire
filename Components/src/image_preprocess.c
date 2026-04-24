/**
 * @file    image_preprocess.c
 * @brief   图像预处理底层运算实现
 * @note    支持低算力平台下的快速色彩空间转换、亮度归一化及像素滤波操作。
 */
#include "image_preprocess.h"
#include <string.h>

/**
 * @brief  RGB565单像素转换为HSV空间向量
 * @param  rgb565 输入的16位像素数据
 * @param  hsv    输出的HSV结构指针
 * @note   采用整数位移算法替代浮点运算，优化单片机算力开销。
 */
void image_preprocess_rgb_to_hsv(uint16_t rgb565, hsv_t* hsv)
{
    uint8_t r, g, b;
    uint8_t max, min, delta;
    
    r = ((rgb565 >> 11) & 0x1F) << 3;
    g = ((rgb565 >> 5) & 0x3F) << 2;
    b = (rgb565 & 0x1F) << 3;
    
    max = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    min = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    delta = max - min;
    
    hsv->v = max;
    
    if(max == 0) {
        hsv->s = 0;
    } else {
        hsv->s = (uint8_t)((delta * 255) / max);
    }
    
    if(delta == 0) {
        hsv->h = 0;
    } else {
        if(max == r) {
            hsv->h = (uint8_t)(((g - b) * 30) / delta);  
            if(g < b) hsv->h += 180;
        } else if(max == g) {
            hsv->h = (uint8_t)(((b - r) * 30) / delta + 60);
        } else {
            hsv->h = (uint8_t)(((r - g) * 30) / delta + 120);
        }
    }
}

/**
 * @brief  处理单行数据的颜色空间转换
 */
void image_preprocess_row_rgb_to_hsv(uint16_t* rgb_row, hsv_t* hsv_row, uint16_t width)
{
    uint16_t i;
    for(i = 0; i < width; i++) {
        image_preprocess_rgb_to_hsv(rgb_row[i], &hsv_row[i]);
    }
}

/**
 * @brief  处理完整图像帧的颜色空间转换
 * @note   支持将外部SRAM中的连续数据映射至检测空间缓冲区。
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
 * @brief  快速3x3窗口中值滤波算法
 * @param  src 输入图像缓冲
 * @param  dst 输出滤波后图像缓冲
 * @note   采用局部数组及基础选择排序寻找中值，边缘像素不处理。
 */
void image_preprocess_median_filter_3x3(uint16_t* src, uint16_t* dst, uint16_t width, uint16_t height)
{
    int16_t x, y;
    uint16_t window[9];
    
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            /* 边缘像素跨越逻辑 */
            if(y == 0 || y == height - 1 || x == 0 || x == width - 1) {
                dst[y * width + x] = src[y * width + x];
                continue;
            }
            
            /* 装填3x3局部窗口数据 */
            window[0] = src[(y-1) * width + (x-1)];
            window[1] = src[(y-1) * width + x];
            window[2] = src[(y-1) * width + (x+1)];
            window[3] = src[y * width + (x-1)];
            window[4] = src[y * width + x];
            window[5] = src[y * width + (x+1)];
            window[6] = src[(y+1) * width + (x-1)];
            window[7] = src[(y+1) * width + x];
            window[8] = src[(y+1) * width + (x+1)];
            
            /* 使用选择排序算法查找3x3窗口的中值（即第五位元素） */
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
            
            dst[y * width + x] = window[4];  
        }
    }
}

/**
 * @brief  单行环境光照归一化算法
 * @note   用于平衡环境强光或背光因素造成的阈值判断误差。
 */
void image_preprocess_brightness_normalize(uint16_t* row, uint16_t width)
{
    uint16_t i;
    uint32_t sum_v = 0;
    uint8_t avg_v;
    
    for(i = 0; i < width; i++) {
        uint16_t rgb = row[i];
        uint8_t r = ((rgb >> 11) & 0x1F) << 3;
        uint8_t g = ((rgb >> 5) & 0x3F) << 2;
        uint8_t b = (rgb & 0x1F) << 3;
        /* 快速估算整体发光强度 */
        sum_v += (r + (g << 1) + b) >> 2;
    }
    
    avg_v = (uint8_t)(sum_v / width);
    
    /* 偏离标准阈值时，执行线性缩放补偿 */
    if(avg_v != 0 && avg_v != 128) {
        uint16_t scale = (128 * 256) / avg_v;  
        
        for(i = 0; i < width; i++) {
            uint16_t rgb = row[i];
            uint8_t r = ((rgb >> 11) & 0x1F);
            uint8_t g = ((rgb >> 5) & 0x3F);
            uint8_t b = (rgb & 0x1F);
            
            r = (r * scale) >> 8;
            g = (g * scale) >> 8;
            b = (b * scale) >> 8;
            
            if(r > 31) r = 31;
            if(g > 63) g = 63;
            if(b > 31) b = 31;
            
            row[i] = (r << 11) | (g << 5) | b;
        }
    }
}
