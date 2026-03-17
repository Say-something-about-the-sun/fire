// jpeg_serial.h - JPEG处理和串口发送集成头文件
#ifndef __JPEG_SERIAL_H
#define __JPEG_SERIAL_H

#include "sys.h"

// 火焰检测配置参数
#define FIRE_DETECT_INTERVAL 3   // 每3帧进行一次火焰检测

// 检测分辨率配置（降低分辨率以提高速度）
#define DETECT_WIDTH   160       // 检测分辨率：160×120
#define DETECT_HEIGHT  120

// 发送分辨率配置（保持原始分辨率）
#define SEND_WIDTH     320       // 发送分辨率：320×240
#define SEND_HEIGHT    240

// 缓冲区状态枚举
typedef enum {
    BUF_IDLE = 0,      // 空闲
    BUF_CAPTURING,     // 采集中
    BUF_SENDING,       // 发送中
    BUF_READY          // 准备好
} BufferState;

// 缓冲区控制结构
typedef struct {
    u8* buffer;           // 缓冲区指针
    BufferState state;     // 缓冲区状态
    u32 data_len;         // 数据长度
    u32 jpeg_start;       // JPEG起始位置
    u32 jpeg_end;         // JPEG结束位置
} BufferControl;

// 函数声明
void serial_display_init(void);
void serial_send_jpeg_data(u8* jpeg_data, u32 jpeg_len);
void jpeg_data_process(void);
void DCMI_StartOneFrame(void);
u32 DCMI_GetFrameLength(void);
u8 process_jpeg_frame(u8 do_fire_detection);  // 添加参数：是否进行火焰检测
u8 jpeg_serial_init(void);
u32 jpeg_serial_get_frame_count(void);
void jpeg_serial_inc_frame_count(void);
u32 jpeg_serial_get_dropped_frames(void);
void jpeg_serial_inc_dropped_frames(void);

// 外部变量声明
extern volatile u8 frame_done;
extern volatile u32 frame_length;
extern volatile u8 dma_transfer_complete;   // DMA传输完成标志
extern __align(4) u8 jpeg_buf_a[];
extern __align(4) u8 jpeg_buf_b[];
extern volatile BufferControl* current_send_buf;  // 当前发送缓冲区

#endif /* __JPEG_SERIAL_H */
