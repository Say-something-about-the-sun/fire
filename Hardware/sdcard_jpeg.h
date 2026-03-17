#ifndef __SDCARD_JPEG_H
#define __SDCARD_JPEG_H

#include "ff.h"
#include "stm32f4xx.h"

// JPEG图像信息
typedef struct {
    u8* data;          // JPEG数据指针
    u32 length;        // JPEG数据长度
    u32 width;         // 图像宽度
    u32 height;        // 图像高度
} JPEG_Image;

// SD卡JPEG保存状态
typedef enum {
    SDCARD_OK = 0,           // 成功
    SDCARD_ERROR,            // 错误
    SDCARD_NO_CARD,          // 无SD卡
    SDCARD_FULL,             // SD卡已满
    SDCARD_WRITE_ERROR,       // 写入错误
    SDCARD_NO_FILESYSTEM      // 无文件系统
} SDCARD_Status;

// 文件命名模式
typedef enum {
    FILENAME_SEQ,            // 顺序编号：image_001.jpg
    FILENAME_TIMESTAMP,       // 时间戳：20250219_120000.jpg
    FILENAME_DETECTION        // 检测类型：safe_001.jpg, fire_001.jpg
} Filename_Mode;

// 函数声明

// 初始化SD卡JPEG保存模块
u8 sdcard_jpeg_init(void);

// 保存JPEG图像到SD卡
SDCARD_Status sdcard_save_jpeg(JPEG_Image* jpeg_img, const char* filename);

// 保存JPEG图像到SD卡（自动命名）
SDCARD_Status sdcard_save_jpeg_auto(JPEG_Image* jpeg_img, Filename_Mode mode);

// 生成文件名
void sdcard_generate_filename(char* filename, u32 count, Filename_Mode mode);

// 获取已保存的文件数量
u32 sdcard_get_file_count(void);

// 删除最旧的文件
SDCARD_Status sdcard_delete_oldest_file(void);

// 格式化SD卡
SDCARD_Status sdcard_format(void);

// 获取SD卡信息
SDCARD_Status sdcard_get_info(u32* total_space, u32* free_space);

// 检查SD卡状态
u8 sdcard_is_ready(void);

// 模式切换函数（解决SDIO和DCMI引脚冲突）
void sw_sdcard_mode(void);  // 切换到SD卡模式
void sw_ov5640_mode(void);  // 切换到摄像头模式

#endif
