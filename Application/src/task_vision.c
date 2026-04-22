// task_vision.c
#include "task_vision.h"
#include "FreeRTOS.h"
#include "task.h"
#include "jpeg_serial.h"
#include "usart2.h"        // ESP32数据处理暂时留在这里
#include "ai_decision.h"   // 向大脑汇报的通道
#include "dcmi.h"
#include "semphr.h"


extern volatile u8 usart1_dma_complete;
// 声明外部的全局串口互斥锁
extern SemaphoreHandle_t Mutex_USART1;
TaskHandle_t VisionTask_Handler;


extern volatile BufferControl* current_capture_buf;
extern volatile BufferControl buf_ctrl_a;
extern volatile BufferControl buf_ctrl_b;
#define JPEG_MAX_SIZE (35*1024)



// 🚨 核心防撕裂标志位：取代危险的 Mutex
volatile u8 g_uart_is_sending_image = 0; 

static void camera_task(void *pvParameters)
{
    static u32 frame_counter = 0;
    FireDetectionResult current_frame_result; 
    
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
    DCMI_CaptureCmd(ENABLE); // 拍第一张！

    while(1)
    {
        // 1. 等待照片拍完
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 
        USART2_Process_ESP32_Data();

        // 2. 找到准备好的数据
        volatile BufferControl* send_buf = NULL;
        if (buf_ctrl_a.state == BUF_READY && current_capture_buf != &buf_ctrl_a) send_buf = &buf_ctrl_a;
        else if (buf_ctrl_b.state == BUF_READY && current_capture_buf != &buf_ctrl_b) send_buf = &buf_ctrl_b;

        if (send_buf != NULL)
        {
            // 3. 裁剪 DMA 垃圾字节（保证 JPEG 结尾绝对干净）
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

            frame_counter++;
            u8 do_fire_detection = (frame_counter >= 5) ? 1 : 0;
            if(frame_counter >= 5) frame_counter = 0; 

            // 4. 🚨 竖起“免打扰”牌子！任何人不准打印日志！
            g_uart_is_sending_image = 1;
            
            current_send_buf = send_buf; 
            if (process_jpeg_frame(do_fire_detection, &current_frame_result) == 0) {
                if (do_fire_detection) AI_Update_Vision_Result(&current_frame_result);
            }
            
            // 5. 等待图片纯净地发完
            u32 timeout = 0;
            while(usart1_dma_complete == 0 && timeout < 1000) { 
                vTaskDelay(pdMS_TO_TICKS(2)); 
                timeout += 2;
            }
            
            send_buf->state = BUF_IDLE;
            current_send_buf = NULL;
            usart1_dma_complete = 0; 
            
            // 6. 图片发完了，放下牌子，允许打印
            g_uart_is_sending_image = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(15)); // 帧间隙
        
        // 🚨 7. 在 SnapShot 模式下，必须手动按下快门，拍下一张！
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
