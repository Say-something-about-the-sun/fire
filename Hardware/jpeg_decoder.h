// jpeg_decoder.h - JPEG解码器头文件

#ifndef __JPEG_DECODER_H
#define __JPEG_DECODER_H

#include "sys.h"

// JPEG缓冲区地址（内部SRAM）
#define JPEG_BUF_ADDR       0x20000000      // 内部SRAM起始地址
#define JPEG_MAX_SIZE       (32*1024)       // 每帧最大32KB

// RGB缓冲区配置（外部SRAM）
#define RGB_BUF_ADDR       0x68000000      // 外部SRAM起始地址
#define RGB_BUF_SIZE       (320*240*2)     // RGB565格式：150KB

// HSV缓冲区配置（外部SRAM）
#define HSV_BUF_ADDR       0x68025800      // RGB缓冲区之后，偏移150KB
#define HSV_BUF_SIZE       (320*240*3)     // HSV格式：225KB

// 解码状态结构
typedef struct {
    uint16_t width;          // 图像宽度
    uint16_t height;         // 图像高度
    uint16_t scale;          // 缩放因子
    uint16_t* dst_buf;       // 目标缓冲区
    uint32_t dst_pos;        // 目标缓冲区位置
    uint8_t  decode_status;  // 解码状态
} JpegDecodeState;
// 函数声明
void jpeg_decoder_init(void);
u8 jpeg_decoder_decode(const uint8_t* jpeg_data, uint32_t jpeg_size);
u8 jpeg_decoder_load_row(uint16_t row, uint8_t buffer_index);
u8 jpeg_decoder_load_rows(uint16_t start_row, uint8_t row_count);
u16 jpeg_decoder_get_pixel(uint8_t buffer_index, uint16_t x, uint16_t y_in_buffer);
void jpeg_decoder_process_with_row_buffer(void);
u8 jpeg_decoder_process(const uint8_t* jpeg_data, uint32_t jpeg_size);
JpegDecodeState* jpeg_decoder_get_state(void);
// 外部变量声明
extern JpegDecodeState decode_state;

#endif /* __JPEG_DECODER_H */