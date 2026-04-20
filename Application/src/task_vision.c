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



// 内部辅助：安全地启动一次新的快照拍照
static void Start_Safe_Snapshot(volatile BufferControl* buf)
{
    // 🚨 核心防死机：在任务里死等 DMA 真正关闭！
    // 此时就算等上几个微秒，也只是阻塞视觉任务，绝对不会卡死系统心跳！
    while((DMA2_Stream1->CR & DMA_SxCR_EN) != RESET);
    
    buf->state = BUF_CAPTURING;
    current_capture_buf = buf;
    
    // 安全地重置 DMA 目标地址
    DMA2_Stream1->M0AR = (u32)buf->buffer;
    DMA2_Stream1->NDTR = JPEG_MAX_SIZE / 4;
    DMA_ClearFlag(DMA2_Stream1, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);
    
    // 重新点火
    DMA_Cmd(DMA2_Stream1, ENABLE);
    DCMI_CaptureCmd(ENABLE); 
}

static void camera_task(void *pvParameters)
{
    static u32 frame_counter = 0;
    FireDetectionResult current_frame_result; 

    // 初始化双盒
    buf_ctrl_a.state = BUF_IDLE;
    buf_ctrl_b.state = BUF_IDLE;

    // 引擎点火：先让 A 盒子去装载照片
    Start_Safe_Snapshot(&buf_ctrl_a);

    while(1)
    {
        // ==========================================
        // 1. 睡觉，等待当前盒子装满照片
        // ==========================================
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 

        // 再次确认 DMA 彻底关闭，准备结算长度
        while((DMA2_Stream1->CR & DMA_SxCR_EN) != RESET);

        // 结算当前盒子的图像长度
        u32 words_rem = DMA_GetCurrDataCounter(DMA2_Stream1);
        u32 img_len = JPEG_MAX_SIZE - (words_rem * 4);
        
        volatile BufferControl* send_buf = NULL;

        // 指针防爆盾：校验数据合法性
        if (img_len > 1000 && img_len <= JPEG_MAX_SIZE) {
            current_capture_buf->data_len = img_len;
            current_capture_buf->state = BUF_READY;
            send_buf = current_capture_buf; // 把这盒准备好的照片拿在手里
        } else {
            current_capture_buf->state = BUF_IDLE; // 坏图，丢弃
        }

        // ==========================================
        // 2. 🚀 流水线精髓：立刻让另一个空盒子去拍照！
        // ==========================================
        volatile BufferControl* next_cap_buf = (current_capture_buf == &buf_ctrl_a) ? &buf_ctrl_b : &buf_ctrl_a;
        if (next_cap_buf->state == BUF_IDLE) {
            // 背景硬件开始默默拍照，完全不占用 CPU 时间！
            Start_Safe_Snapshot(next_cap_buf);
        }

        // ==========================================
        // 3. 慢慢处理和发送刚刚拿在手里的 send_buf
        // ==========================================
        if (send_buf != NULL && send_buf->state == BUF_READY)
        {
            // 裁剪 DMA 垃圾字节防花屏
            u32 len = send_buf->data_len;
            u8 *img_ptr = (u8*)send_buf->buffer;
            for(int i = 0; i < 8; i++) {
                if(img_ptr[len - 1 - i] == 0xD9 && img_ptr[len - 2 - i] == 0xFF) {
                    send_buf->data_len = len - i; 
                    break;
                }
            }

            frame_counter++;
            u8 do_fire_detection = (frame_counter >= 5) ? 1 : 0;
            if(frame_counter >= 5) frame_counter = 0; 

            // 竖起免打扰牌子，锁定串口发送
            g_uart_is_sending_image = 1;
            
            if(Mutex_USART1 != NULL) xSemaphoreTake(Mutex_USART1, portMAX_DELAY);
            current_send_buf = send_buf; 
            
            if (process_jpeg_frame(do_fire_detection, &current_frame_result) == 0) {
                if (do_fire_detection) AI_Update_Vision_Result(&current_frame_result);
            }
            
            // 死等 DMA 串口发送完毕（耗时约 100ms）
            // ⚡ 注意：就在我们等串口的时候，另一个盒子的照片已经在背景拍好了！
            u32 timeout = 0;
            while(usart1_dma_complete == 0 && timeout < 1000) { 
                vTaskDelay(pdMS_TO_TICKS(2)); 
                timeout += 2;
            }
            
            current_send_buf = NULL;
            usart1_dma_complete = 0; 
            if(Mutex_USART1 != NULL) xSemaphoreGive(Mutex_USART1);

            g_uart_is_sending_image = 0;
            
            // 发完收工，将手里的盒子清空，留给下一轮使用
            send_buf->state = BUF_IDLE; 
        }
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
