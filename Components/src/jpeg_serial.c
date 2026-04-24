/**
 * @file    jpeg_serial.c
 * @brief   底层图像转发控制中心与双缓存轮转实现
 * @note    聚合了 DCMI 数据捕获的协调管控，管理视频流向客户端转发，
 * 并在周期分频时刻触发底层视觉识别。
 */
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "dcmi.h"
#include "jpeg_decoder.h"
#include "jpeg_serial.h"
#include "image_preprocess.h"
#include "fire_detection.h"
#include "led.h"
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

extern SemaphoreHandle_t Mutex_USART1;
void Safe_Printf(char *format, ...);

#define DETECT_RGB_BUF_ADDR 0x6804B000      
#define DETECT_HSV_BUF_ADDR 0x68050800      

/* 内存驻留设置与管理状态块初始化 */
__align(4) u8 jpeg_buf_a[JPEG_MAX_SIZE];  
__align(4) u8 jpeg_buf_b[JPEG_MAX_SIZE];  

volatile BufferControl buf_ctrl_a = {jpeg_buf_a, BUF_IDLE, 0, 0, 0};
volatile BufferControl buf_ctrl_b = {jpeg_buf_b, BUF_IDLE, 0, 0, 0};
volatile BufferControl* current_capture_buf = &buf_ctrl_a;  
volatile BufferControl* current_send_buf = NULL;             

typedef struct {
    u32 length;          
    u8  valid;          
} FrameInfo;

volatile FrameInfo frame_info;
volatile u8 frame_valid = 0;        

volatile u8 jpeg_data_ok = 0;
volatile u32 jpeg_data_len = 0;
volatile u8 ovx_mode = 0x01;  

volatile u8 frame_done = 0;
volatile u32 frame_length = 0;

volatile u32 dropped_frames = 0;
volatile u32 frame_count = 0;
volatile u32 g_processed_frame_count = 0;  
volatile u32 fire_detection_frame_count = 0;  

#define FIRE_DETECTION_INTERVAL 20  

extern u8 ov_frame;  

volatile u8 dma_transfer_complete = 0;     
volatile u32 dma_transfer_count = 0;       
volatile BufferControl* completed_buffer = NULL;  

/*
 * 注意：JPEG 模式下数据长度不固定，DMA TCIF 可能永远不会触发，
 * 因此由 DCMI 帧中断接管后续处理。
 */
void DMA2_Stream1_IRQHandler(void)
{
    DMA_ClearITPendingBit(DMA2_Stream1, DMA_IT_TCIF1 | DMA_IT_HTIF1 | DMA_IT_TEIF1 | DMA_IT_DMEIF1 | DMA_IT_FEIF1);
}

void switch_buffer_and_restart_dma(void)
{
    if(completed_buffer == NULL) {
        return;
    }
    
    if(current_capture_buf == &buf_ctrl_a) {
        current_capture_buf = &buf_ctrl_b;
    } else {
        current_capture_buf = &buf_ctrl_a;
    }
    
    if(current_capture_buf->state != BUF_IDLE) {
        dropped_frames++;
        current_capture_buf->state = BUF_IDLE;
    }
    
    current_capture_buf->state = BUF_CAPTURING;
    
    DMA_Cmd(DMA2_Stream1, DISABLE);
    while(DMA_GetCmdStatus(DMA2_Stream1) != DISABLE);
    
    DMA2_Stream1->M0AR = (u32)current_capture_buf->buffer;
    DMA2_Stream1->NDTR = JPEG_MAX_SIZE / 4;
    
    DMA_ClearFlag(DMA2_Stream1, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);
    
    DMA_Cmd(DMA2_Stream1, ENABLE);
    DCMI_CaptureCmd(ENABLE);
    
    completed_buffer = NULL;
    dma_transfer_complete = 0;
}

void serial_display_init(void)
{
    printf("[Serial Display] Initialized\r\n");
    printf("[Serial Display] Ready to send JPEG data\r\n");
}

void downsample_rgb565(u16* src, u16 src_w, u16 src_h, u16* dst, u16 dst_w, u16 dst_h)
{
    u16 x, y, dx, dy;
    u32 r, g, b;
    u16 pixel;
    u16 src_x, src_y;
    
    for(y = 0; y < dst_h; y++)
    {
        for(x = 0; x < dst_w; x++)
        {
            src_x = x * 2;
            src_y = y * 2;
            
            r = 0; g = 0; b = 0;
            for(dy = 0; dy < 2; dy++)
            {
                for(dx = 0; dx < 2; dx++)
                {
                    pixel = src[(src_y + dy) * src_w + (src_x + dx)];
                    r += (pixel >> 11) & 0x1F;
                    g += (pixel >> 5) & 0x3F;
                    b += pixel & 0x1F;
                }
            }
            
            r /= 4;
            g /= 4;
            b /= 4;
            
            dst[y * dst_w + x] = (r << 11) | (g << 5) | b;
        }
    }
}

/**
 * @brief  封装串口传输规约并启用 DMA 下行通讯
 */
void serial_send_jpeg_data(u8* jpeg_data, u32 jpeg_len)
{
    u32 i;
    u8 checksum = 0;
    
    for(i = 0; i < jpeg_len; i++)
    {
        checksum += jpeg_data[i];
    }
    
    uart_send_byte(0x00);
    uart_send_byte(0xFF);
    uart_send_byte(0x01);
    
    uart_send_byte((jpeg_len >> 24) & 0xFF);
    uart_send_byte((jpeg_len >> 16) & 0xFF);
    uart_send_byte((jpeg_len >> 8) & 0xFF);
    uart_send_byte(jpeg_len & 0xFF);
    
    usart1_dma_send(jpeg_data, (uint16_t)jpeg_len);
}


/**
 * @brief  复位底层状态，捕获全新视频帧
 */
void DCMI_StartOneFrame(void)
{
    frame_done = 0;
    current_capture_buf->state = BUF_CAPTURING;

    DMA_Cmd(DMA2_Stream1, DISABLE);
    while(DMA_GetCmdStatus(DMA2_Stream1) != DISABLE);

    DMA2_Stream1->M0AR = (u32)current_capture_buf->buffer;
    DMA2_Stream1->NDTR = JPEG_MAX_SIZE / 4;

    DMA_ClearFlag(DMA2_Stream1,
                  DMA_FLAG_TCIF1 |
                  DMA_FLAG_HTIF1 |
                  DMA_FLAG_TEIF1 |
                  DMA_FLAG_DMEIF1 |
                  DMA_FLAG_FEIF1);

    DMA_Cmd(DMA2_Stream1, ENABLE);
    DCMI_CaptureCmd(ENABLE);
}

u32 DCMI_GetFrameLength(void)
{
    u32 words;

    DMA_Cmd(DMA2_Stream1, DISABLE);
    while(DMA_GetCmdStatus(DMA2_Stream1) != DISABLE);

    words = (JPEG_MAX_SIZE / 4) - DMA_GetCurrDataCounter(DMA2_Stream1);
    current_capture_buf->data_len = words * 4;
    current_capture_buf->state = BUF_READY;
    
    return current_capture_buf->data_len;
}

/**
 * @brief  全栈流处理链路集成函数 (校验定位、传输分发与分析联动)
 * @param  do_fire_detection 当允许执行高能耗分析时开启
 * @param  out_result        提取处理后的诊断标签
 */
u8 process_jpeg_frame(u8 do_fire_detection, FireDetectionResult* out_result)
{
    frame_count++;
    
    volatile BufferControl* ready_buf = NULL;
    if(buf_ctrl_a.state == BUF_READY) {
        ready_buf = &buf_ctrl_a;
    } else if(buf_ctrl_b.state == BUF_READY) {
        ready_buf = &buf_ctrl_b;
    } else {
        return 1;  
    }
    
    ready_buf->state = BUF_SENDING;
    u8* p = ready_buf->buffer;
    u32 jpeg_start = 0, jpeg_end = 0, jpeg_size = 0;
    frame_length = ready_buf->data_len;
    
    if(frame_length > 0 && frame_length <= JPEG_MAX_SIZE) {
        /* 特征起始序列提取 */
        for(u32 i = 0; i < 512 && i < frame_length - 1; i++) {
            if(p[i] == 0xFF && p[i+1] == 0xD8) {
                jpeg_start = i; break;
            }
        }
        
        if(jpeg_start == 0 && (p[0] != 0xFF || p[1] != 0xD8)) {
            ready_buf->state = BUF_IDLE;
            return 1;
        }
        
        /* 终结符扫描 */
        for(u32 i = jpeg_start; i < frame_length - 1; i++) {
            if(p[i] == 0xFF && p[i+1] == 0xD9) {
                jpeg_end = i + 2; break;
            }
        }
        
        if(jpeg_end > jpeg_start) {
            jpeg_size = jpeg_end - jpeg_start;
        } else {
            jpeg_size = frame_length - jpeg_start;
        }
        
        if(jpeg_size > 100) {
            /* 持有关键通讯互斥量 */
            if(Mutex_USART1 != NULL) xSemaphoreTake(Mutex_USART1, portMAX_DELAY);
            
            serial_send_jpeg_data(&p[jpeg_start], jpeg_size);
            
            /* 硬件状态轮询：查询 DMA 使能位状态，避免依赖软件中断标志位可能带来的状态不同步 */
            u32 dma_wait = 0;
            while(DMA_GetCmdStatus(DMA2_Stream7) != DISABLE && dma_wait < 100) { 
                vTaskDelay(pdMS_TO_TICKS(2)); 
                dma_wait++; 
            }
            
            if(Mutex_USART1 != NULL) xSemaphoreGive(Mutex_USART1);
            
            /* 通信帧间隔缓冲防粘连 */
            vTaskDelay(pdMS_TO_TICKS(20));
            g_processed_frame_count++;
            
            /* 系统工作状况外部表现 */
            static u8 led_state = 0;
            if(led_state == 0) { led_on(led0); led_state = 1; } 
            else { led_off(led0); led_state = 0; }
            
            /* 算力限流器：根据预设帧间隔执行 AI 算法，避免长期占用 CPU */
            if(do_fire_detection) {
                /* 更新检测计数，确保进入取余逻辑 */
                fire_detection_frame_count++; 
                
                if(fire_detection_frame_count % 5 == 0) { 
                    int decode_res = jpeg_decoder_process(&p[jpeg_start], jpeg_size);
                    if(decode_res == 0) {
                        JpegDecodeState* dec = jpeg_decoder_get_state();
                        image_preprocess_frame_rgb_to_hsv(dec->dst_buf, 
                                                         (hsv_t*)DETECT_HSV_BUF_ADDR, 
                                                         dec->width, dec->height);
                        
                        FireDetectionResult fire_result;
                        fire_detection_process_frame((hsv_t*)DETECT_HSV_BUF_ADDR, 
                                                    dec->width, dec->height, 
                                                    &fire_result);
                        
                        if (out_result != NULL) {
                            *out_result = fire_result;
                        }
                    }
                }
            }
        }
    }
    
    /* 缓冲区回放收尾及设备再次武装 */
    ready_buf->state = BUF_IDLE;
    DCMI_StartOneFrame();
    return 0;
}

/**
 * @brief  初始化底层图像捕捉模块及其缓冲链路
 */
u8 jpeg_serial_init(void)
{
    memset(jpeg_buf_a, 0, JPEG_MAX_SIZE);
    memset(jpeg_buf_b, 0, JPEG_MAX_SIZE);
    
    buf_ctrl_a.state = BUF_IDLE;
    buf_ctrl_b.state = BUF_IDLE;
    current_capture_buf = &buf_ctrl_a;
    current_send_buf = NULL;
    
    frame_info.length = 0;
    frame_info.valid = 0;
    frame_valid = 0;
    dropped_frames = 0;
    frame_count = 0;
    
    DCMI_DMA_Init((u32)jpeg_buf_a, JPEG_MAX_SIZE / 4, DMA_MemoryDataSize_Word, DMA_MemoryInc_Enable);
    
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    DMA_ITConfig(DMA2_Stream1, DMA_IT_TC, ENABLE);
    
    jpeg_decoder_init();
    
    FireDetectionConfig fire_config;
    fire_config.h_min = 0;
    fire_config.h_max = 35;
    fire_config.s_min = 80;
    fire_config.v_min = 100;
    fire_config.min_area = 12;  
    fire_config.flicker_threshold = 3;
    fire_detection_init(&fire_config);
    printf("[JPEG Serial] Fire Detection initialized!\r\n");
    
    printf("[JPEG Serial] Initializing Serial Display...\r\n");
    serial_display_init();
    
    printf("[JPEG Serial] Initializing USART1 DMA TX...\r\n");
    usart1_dma_init();
    printf("[JPEG Serial] USART1 DMA TX initialized!\r\n");
    
    printf("[JPEG Serial] Starting first frame capture...\r\n");
    DCMI_StartOneFrame();
    printf("[JPEG Serial] First frame capture started!\r\n");
    
    printf("[JPEG Serial] System initialized successfully!\r\n");
    printf("[JPEG Serial] Dual buffer mode enabled\r\n");
    
    return 0;
}

u32 jpeg_serial_get_frame_count(void)
{
    return frame_count;
}

void jpeg_serial_inc_frame_count(void)
{
    frame_count++;
}

u32 jpeg_serial_get_dropped_frames(void)
{
    return dropped_frames;
}

void jpeg_serial_inc_dropped_frames(void)
{
    dropped_frames++;
}
