/**
 * @file    jpeg_decoder.c
 * @brief   JPEG 格式软件解码核心及缓存管理调度
 * @note    基于 TJpgDec 库封装，利用外部 SRAM 作为 RGB 数据容纳区，
 * 并通过内部 SRAM 构建行缓冲机制以弥补总线访问性能。
 */
#include "sys.h"
#include "delay.h"
#include "tjpgd.h"
#include "extsram.h"
#include <stdio.h>

#define RGB_BUF_SIZE       (320 * 240 * 2)       
#define RGB_BUF_ADDR       0x68000000            

/* 内部SRAM行缓冲性能参数配置 */
#define ROW_BUFFER_HEIGHT  8                      
#define ROW_BUFFER_WIDTH   320                    
#define ROW_BUFFER_SIZE    (ROW_BUFFER_WIDTH * ROW_BUFFER_HEIGHT * 2)  
#define JPEG_MAX_SIZE      (35*1024)

extern uint8_t jpeg_buf_a;

#define rgb_buffer  ((u16*)RGB_BUF_ADDR)  

__align(4) u16 row_buffer[ROW_BUFFER_HEIGHT][ROW_BUFFER_WIDTH];

/* 将TJpgDec解码工作栈锁定于高性能的CCM内部内存区域 */
u8 work[16384] __attribute__((at(0x10000000)));

typedef struct {
    uint16_t width;          
    uint16_t height;         
    uint16_t scale;          
    uint16_t* dst_buf;       
    uint32_t dst_pos;        
    uint8_t  decode_status;  
} JpegDecodeState;

typedef struct {
    const uint8_t* data;  
    uint32_t size;        
    uint32_t offset;      
} jpeg_stream_t;

static JpegDecodeState decode_state;

/**
 * @brief  数据载入回调函数 (被TJpgDec内核调用)
 */
static uint32_t input_func(JDEC* jd, uint8_t* buff, uint32_t nbyte)
{
    jpeg_stream_t* stream = (jpeg_stream_t*)jd->device;
    uint32_t remain = stream->size - stream->offset;
    
    if (buff) {
        uint32_t rd = (nbyte < remain) ? nbyte : remain;
        if (rd > 0) {
            memcpy(buff, stream->data + stream->offset, rd);
        }
        stream->offset += rd;
        return rd;
    } else {
        uint32_t sk = (nbyte < remain) ? nbyte : remain;
        stream->offset += sk;
        return sk;
    }
}

/**
 * @brief  解码数据投递回调函数 (被TJpgDec内核调用)
 */
static int output_func(JDEC* jd, void* bitmap, JRECT* rect)
{
    uint16_t* src = (uint16_t*)bitmap;
    uint16_t* dst;
    uint16_t y, x;
    
    for (y = rect->top; y <= rect->bottom; y++) {
        dst = decode_state.dst_buf + y * decode_state.width + rect->left;
        for (x = rect->left; x <= rect->right; x++) {
            *dst++ = *src++;
        }
    }
    return 1; 
}

void jpeg_decoder_init(void)
{
    Extsram_Init();
    
    decode_state.width = 0;
    decode_state.height = 0;
    decode_state.scale = 0;
    decode_state.dst_buf = (uint16_t*)RGB_BUF_ADDR;
    decode_state.dst_pos = 0;
    decode_state.decode_status = 0;
    
    // printf("[JPEG Decoder] Initialized with:\r\n");
    // ...
}

/**
 * @brief  执行数据流的完整解码逻辑
 * @param  jpeg_data 图像起始游标
 * @param  jpeg_size 数据长度
 * @return u8 执行状态码
 */
u8 jpeg_decoder_decode(const uint8_t* jpeg_data, uint32_t jpeg_size)
{
    JDEC jdec;
    uint8_t result;
    jpeg_stream_t stream;

    /*
    // 调试探针：检查是否发生 DMA 内存覆写 (Memory Overlap)
    printf("\r\n--- MEMORY CHECK ---\r\n");
    printf("JPEG Data Addr (DMA) : 0x%08X\r\n", (uint32_t)jpeg_data);
    printf("TJpgDec Work Addr    : 0x%08X\r\n", (uint32_t)work);
    
    int diff = (int)((uint32_t)work - (uint32_t)jpeg_data);
    if(diff < 0) diff = -diff;
    printf("Distance             : %d bytes\r\n", diff);
    
    if(diff < 32768) {
        printf("DANGER: Memory Overlap Detected! DMA is crushing the workspace!\r\n");
    }
    printf("--------------------\r\n");
    */

    stream.data = jpeg_data;      
    stream.size = jpeg_size;
    stream.offset = 0;
    
    result = jd_prepare(&jdec, input_func, work, sizeof(work), (void*)&stream);
        
    if (result != JDR_OK) {
        printf("[JPEG Decoder] Prepare failed: %d\r\n", result);
        return result;
    }
    
    /* 根据下采样比例重定义输出参数 */
    decode_state.width = jdec.width / 2;
    decode_state.height = jdec.height / 2;
    decode_state.scale = 1;  
    
    result = jd_decomp(&jdec, output_func, decode_state.scale);
    if (result != JDR_OK) {
        printf("[JPEG Decoder] Decompress failed: %d\r\n", result);
        return result;
    }
    
    decode_state.decode_status = 1;
    
    return JDR_OK;
}

/**
 * @brief  将指定索引数据迁入高速行缓冲
 */
u8 jpeg_decoder_load_row(uint16_t row, uint8_t buffer_index)
{
    if (row >= decode_state.height || buffer_index >= ROW_BUFFER_HEIGHT) {
        return 1;
    }
    
    uint16_t* src = (uint16_t*)RGB_BUF_ADDR + row * decode_state.width;
    uint16_t* dst = row_buffer[buffer_index];
    
    /* 利用FSMC总线机制进行批量内存转储提升传输速率 */
    for (uint16_t x = 0; x < decode_state.width; x++) {
        *dst++ = *src++;
    }
    return 0;
}

/**
 * @brief  将大块数据批量加载至行缓冲
 */
u8 jpeg_decoder_load_rows(uint16_t start_row, uint8_t row_count)
{
    if (start_row + row_count > decode_state.height || row_count > ROW_BUFFER_HEIGHT) {
        return 1;
    }
    
    for (uint8_t i = 0; i < row_count; i++) {
        if (jpeg_decoder_load_row(start_row + i, i) != 0) {
            return 1;
        }
    }
    return 0;
}

u16 jpeg_decoder_get_pixel(uint8_t buffer_index, uint16_t x, uint16_t y_in_buffer)
{
    if (buffer_index >= ROW_BUFFER_HEIGHT || 
        x >= decode_state.width || 
        y_in_buffer >= ROW_BUFFER_HEIGHT) {
        return 0;
    }
    return row_buffer[buffer_index][x];
}

u16* jpeg_decoder_get_rgb_buffer(void)
{
    return (u16*)RGB_BUF_ADDR;
}

JpegDecodeState* jpeg_decoder_get_state(void)
{
    return &decode_state;
}

/**
 * @brief  调度行缓冲区应用底层图像算法
 * @return u8 执行状态码
 */
u8 jpeg_decoder_process_with_row_buffer(void)
{
    if (!decode_state.decode_status) {
        printf("[JPEG Decoder] No decoded image\r\n");
        return 1;
    }
    
    uint16_t row, x;
    uint8_t buffer_index;
    uint32_t process_time = 0;
    
    for (row = 0; row < decode_state.height; row += ROW_BUFFER_HEIGHT) {
        uint8_t rows_to_process = ROW_BUFFER_HEIGHT;
        if (row + rows_to_process > decode_state.height) {
            rows_to_process = decode_state.height - row;
        }
        
        uint32_t load_start = millis();
        if (jpeg_decoder_load_rows(row, rows_to_process) != 0) {
            return 1;
        }
        process_time += millis() - load_start;
        
        for (buffer_index = 0; buffer_index < rows_to_process; buffer_index++) {
            
            uint32_t sum_r = 0, sum_g = 0, sum_b = 0;
            for (x = 0; x < decode_state.width; x++) {
                u16 pixel = row_buffer[buffer_index][x];
                u8 r = (pixel >> 11) & 0x1F;
                u8 g = (pixel >> 5) & 0x3F;
                u8 b = pixel & 0x1F;
                sum_r += r;
                sum_g += g;
                sum_b += b;
            }
        }
    }
    return 0;
}

/**
 * @brief  上层解码业务与后续处理联合接口
 */
u8 jpeg_decoder_process(const uint8_t* jpeg_data, uint32_t jpeg_size)
{
    if(jpeg_decoder_decode(jpeg_data, jpeg_size) != 0) {
        return 1;
    }
    jpeg_decoder_process_with_row_buffer();
    return 0;
}

void jpeg_decoder_clear(void)
{
    memset(&decode_state, 0, sizeof(JpegDecodeState));
    memset(rgb_buffer, 0, sizeof(rgb_buffer));
    memset(row_buffer, 0, sizeof(row_buffer));
}
