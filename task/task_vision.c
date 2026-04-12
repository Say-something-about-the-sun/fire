// task_vision.c
#include "task_vision.h"
#include "FreeRTOS.h"
#include "task.h"
#include "jpeg_serial.h"
#include "usart2.h"        // ESP32数据处理暂时留在这里
#include "ai_decision.h"   // 向大脑汇报的通道


extern volatile u8 usart1_dma_complete;


 TaskHandle_t VisionTask_Handler;

static void camera_task(void *pvParameters)
{
    static u32 frame_counter = 0;
    FireDetectionResult current_frame_result; 
    
    // 🚨 修复 3：强行接管并降低 DCMI 中断优先级到 FreeRTOS 安全线（5级）
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DCMI_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; 
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 第一次扣动扳机，启动系统
    DCMI_CaptureCmd(ENABLE);

    while(1)
    {
        // 1. 睡觉，等待硬件采完一张图
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 

        USART2_Process_ESP32_Data();
        
        frame_counter++;
        u8 do_fire_detection = (frame_counter >= 5) ? 1 : 0;
        if(frame_counter >= 5) frame_counter = 0; 
        
        // 2. 解码、AI推理、通过串口 DMA 发送
        if (process_jpeg_frame(do_fire_detection, &current_frame_result) == 0)
        {
            if (do_fire_detection) {
                AI_Update_Vision_Result(&current_frame_result);
            }
        }
        
        // 3. 收尾工作与下一帧请求
        if(current_send_buf != NULL)
        {
            u32 timeout = 0;
            // 🚨 架构级修复 2：耐心等待这张图在串口上彻彻底底发完
            while(usart1_dma_complete == 0 && timeout < 1000) { // 放宽超时到 1000ms，因为发图很慢！
                vTaskDelay(pdMS_TO_TICKS(2)); 
                timeout += 2;
            }
            
            // 释放缓冲区
            current_send_buf->state = BUF_IDLE;
            current_send_buf = NULL;
            usart1_dma_complete = 0; 
        }

        // 🚨 架构级修复 3：一切处理干净了，主动请求拍下一张！
        // 这样一来，不管你处理多慢，都不会有任何内存被强行覆盖！
        DCMI_CaptureCmd(ENABLE);
    }
}

void Vision_Task_Init(void)
{
    xTaskCreate((TaskFunction_t)camera_task,             
                (const char* )"task_vision",           
                (uint16_t      )1024,              
                (void* )NULL,                  
                (UBaseType_t   )3,                 
                (TaskHandle_t* )&VisionTask_Handler); 
}
