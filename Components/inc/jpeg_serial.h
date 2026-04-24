/**
 * @file    jpeg_serial.h
 * @brief   视频流底层缓冲与串口发送控制调度头文件
 */
#ifndef __JPEG_SERIAL_H
#define __JPEG_SERIAL_H

#include "sys.h"
#include "fire_detection.h"

/* AI检测控制分频与图像分析分辨率定义 */
#define FIRE_DETECT_INTERVAL 3   
#define DETECT_WIDTH   160       
#define DETECT_HEIGHT  120
#define SEND_WIDTH     320       
#define SEND_HEIGHT    240

/* 硬件双缓冲状态机抽象控制字 */
typedef enum {
    BUF_IDLE = 0,      
    BUF_CAPTURING,     
    BUF_SENDING,       
    BUF_READY          
} BufferState;

/* DMA内存池单元管理结构 */
typedef struct {
    u8* buffer;           
    BufferState state;     
    u32 data_len;         
    u32 jpeg_start;       
    u32 jpeg_end;         
} BufferControl;

/* 对外功能函数与操作接口声明 */
void serial_display_init(void);
void serial_send_jpeg_data(u8* jpeg_data, u32 jpeg_len);
void jpeg_data_process(void);
void DCMI_StartOneFrame(void);
u32  DCMI_GetFrameLength(void);

u8   process_jpeg_frame(u8 do_fire_detection, FireDetectionResult *out_result);
u8   jpeg_serial_init(void);
u32  jpeg_serial_get_frame_count(void);
void jpeg_serial_inc_frame_count(void);
u32  jpeg_serial_get_dropped_frames(void);
void jpeg_serial_inc_dropped_frames(void);

/* 跨模块通信标志量与内存池暴露 */
extern volatile u8 frame_done;
extern volatile u32 frame_length;
extern volatile u8 dma_transfer_complete;   
extern __align(4) u8 jpeg_buf_a[];
extern __align(4) u8 jpeg_buf_b[];
extern volatile BufferControl* current_send_buf;  

#endif /* __JPEG_SERIAL_H */
