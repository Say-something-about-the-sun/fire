/**
 * @file    jpeg_decoder.h
 * @brief   JPEG 数据流软件解码器接口
 * @note    支持动态内存池与外部SRAM地址映射的高速解码处理。
 */
#ifndef __JPEG_DECODER_H
#define __JPEG_DECODER_H

#include "sys.h"

#define JPEG_MAX_SIZE       (35*1024)       /* 单帧缓冲限制：35KB */

/* 外部大容量高速SRAM(FSMC)映射地址配置 */
#define RGB_BUF_ADDR       0x68000000      /* 外部SRAM起始基地址 */
#define RGB_BUF_SIZE       (320*240*2)     /* 映射存储容量：150KB */

#define HSV_BUF_ADDR       0x68025800      /* HSV向量空间起始地址 */
#define HSV_BUF_SIZE       (320*240*3)     /* HSV存储容量：225KB */

/* 解码器控制状态抽象 */
typedef struct {
    uint16_t width;          /* 像素宽度 */
    uint16_t height;         /* 像素高度 */
    uint16_t scale;          /* 等比缩放系数 */
    uint16_t* dst_buf;       /* 解码后数据落盘地址 */
    uint32_t dst_pos;        /* 当前填充游标 */
    uint8_t  decode_status;  /* 任务执行状态 */
} JpegDecodeState;

/* 外部功能接口声明 */
void jpeg_decoder_init(void);
u8   jpeg_decoder_decode(const uint8_t* jpeg_data, uint32_t jpeg_size);
u8   jpeg_decoder_load_row(uint16_t row, uint8_t buffer_index);
u8   jpeg_decoder_load_rows(uint16_t start_row, uint8_t row_count);
u16  jpeg_decoder_get_pixel(uint8_t buffer_index, uint16_t x, uint16_t y_in_buffer);
void jpeg_decoder_process_with_row_buffer(void);
u8   jpeg_decoder_process(const uint8_t* jpeg_data, uint32_t jpeg_size);
JpegDecodeState* jpeg_decoder_get_state(void);

extern JpegDecodeState decode_state;

#endif /* __JPEG_DECODER_H */
