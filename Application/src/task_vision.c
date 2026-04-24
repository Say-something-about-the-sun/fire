/**
 * @file    task_vision.c
 * @brief   图像捕获协同与机器视觉链路任务实现
 * @note    集成摄像头硬件流驱动，处理乒乓缓冲调度并触发 JPEG 推流与图像算法分析。
 */
#include "task_vision.h"
#include "FreeRTOS.h"
#include "task.h"
#include "jpeg_serial.h"
#include "usart2.h"        
#include "ai_decision.h"   
#include "dcmi.h"
#include "semphr.h"

extern volatile u8 usart1_dma_complete;
extern SemaphoreHandle_t Mutex_USART1;
TaskHandle_t VisionTask_Handler;

extern volatile BufferControl* current_capture_buf;
extern volatile BufferControl buf_ctrl_a;
extern volatile BufferControl buf_ctrl_b;
#define JPEG_MAX_SIZE (35*1024)

/* 串口推流核心防冲突标志位，用于规避推流期间的日志混叠 */
volatile u8 g_uart_is_sending_image = 0; 

/**
 * @brief  摄像头采集与算法调度系统级线程
 * @param  pvParameters 传入参数 (未使用)
 */
static void camera_task(void *pvParameters)
{
    static u32 frame_counter = 0;
    FireDetectionResult current_frame_result; 
    
    /* 对齐 DCMI 硬件中断优先级与 RTOS 体系 */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DCMI_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; 
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    if (current_capture_buf == NULL) {
        current_capture_buf = &buf_ctrl_a; 
    }

    DMA2_Stream1->M0AR = (u32)current_capture_buf->buffer;
    DMA2_Stream1->NDTR = JPEG_MAX_SIZE / 4;
    DMA_ClearFlag(DMA2_Stream1, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);
    DMA_Cmd(DMA2_Stream1, ENABLE);
    DCMI_CaptureCmd(ENABLE); 

    while(1)
    {
        /* 1. 通过 RTOS 任务通知，等待 DCMI 帧结束信号唤醒 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 
        USART2_Process_ESP32_Data();

        /* 2. 状态机切换，寻址有效帧数据缓冲 */
        volatile BufferControl* send_buf = NULL;
        if (buf_ctrl_a.state == BUF_READY && current_capture_buf != &buf_ctrl_a) send_buf = &buf_ctrl_a;
        else if (buf_ctrl_b.state == BUF_READY && current_capture_buf != &buf_ctrl_b) send_buf = &buf_ctrl_b;

        if (send_buf != NULL)
        {
            /* 3. 剥离 DMA 通道残留的截断噪音，保证图像流的规范结束 */
            if(send_buf->data_len > 2) {
                u32 len = send_buf->data_len;
                u8 *img_ptr = (u8*)send_buf->buffer;
                for(int i = 0; i < 8; i++) {
                    if(img_ptr[len - 1 - i] == 0xD9 && img_ptr[len - 2 - i] == 0xFF) {
                        send_buf->data_len = len - i; 
                        break;
                    }
                }
            }

            /* 依据算力分配设定算法调度频次 */
            frame_counter++;
            u8 do_fire_detection = (frame_counter >= 5) ? 1 : 0;
            if(frame_counter >= 5) frame_counter = 0; 

            /* 4. 拉起图像发送互斥屏蔽标记，遏制其他任务的数据日志干预 */
            g_uart_is_sending_image = 1;
            
            current_send_buf = send_buf; 
            if (process_jpeg_frame(do_fire_detection, &current_frame_result) == 0) {
                if (do_fire_detection) AI_Update_Vision_Result(&current_frame_result);
            }
            
            /* 5. 配合硬件总线寄存器进行非阻塞的传输交接验证 */
            u32 timeout = 0;
            while(usart1_dma_complete == 0 && timeout < 1000) { 
                vTaskDelay(pdMS_TO_TICKS(2)); 
                timeout += 2;
            }
            
            send_buf->state = BUF_IDLE;
            current_send_buf = NULL;
            usart1_dma_complete = 0; 
            
            /* 6. 传输结束，撤销互斥屏蔽 */
            g_uart_is_sending_image = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(40)); /* 插入必要帧间隔，维护总线时序稳定性 */
        
        /* 7. 重置 DCMI 流控制器并激活下一帧单拍采集 (SnapShot模式要求) */
        DCMI_CaptureCmd(ENABLE);
    }
}

/**
 * @brief  视觉处理系统级任务初始化配置
 */
void Vision_Task_Init(void)
{
    xTaskCreate((TaskFunction_t)camera_task,             
                (const char* )"task_vision",           
                (uint16_t      )1024,              
                (void* )NULL,                  
                (UBaseType_t   )3,                 
                (TaskHandle_t* )&VisionTask_Handler); 
}
