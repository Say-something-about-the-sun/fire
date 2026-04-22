// main.c - 主程序 - 工业级清爽版
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "usart2.h"
#include "usart3.h"

// 硬件驱动层 (BSP)
#include "led.h"
#include "key.h"
#include "extsram.h"
#include "RTC.h"
#include "dht11.h"
#include "smoke.h"
#include "water_pump.h"
#include "ov5640.h"
#include "dcmi.h"
#include "ESP8266.h"
#include "iwdg.h"

// 业务组件层
#include "jpeg_serial.h"

// RTOS与核心调度层
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ai_decision.h"
#include "task_hmi.h"
#include "task_network.h"
#include "task_vision.h"

#include <stdio.h>
#include <stdarg.h>

// 全局互斥锁与标志位
SemaphoreHandle_t Mutex_USART1;
extern volatile u8 g_uart_is_sending_image; 

// 线程安全的纯净打印输出
void Safe_Printf(char *format, ...)
{
	/*
    if (g_uart_is_sending_image == 1) return; // 保护图像通道
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
	*/
	return;
}

// 摄像头参数宏
#define CAMERA_WIDTH  320
#define CAMERA_HEIGHT 240

// 任务句柄
TaskHandle_t StartTask_Handler;
void start_task(void *pvParameters);

int main(void)
{
    // 1. 基础系统时钟与外设初始化
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    delay_init(168);
    uart_init(921600);
    
    // 2. 纯硬件设备初始化
    LED_Init();
    KEY_Init();
    Extsram_Init();
    RTC_Init_Custom();
    DHT11_Init();
    Smoke_Sensor_Init();
    WaterPump_Init();
    
    // 3. 通信总线初始化
    USART2_Init(115200);
    USART2_ConfigInterrupt();
    USART3_Init(9600);  
    USART3_ConfigInterrupt();  
    
    // 4. 摄像头与 JPEG 系统初始化
    jpeg_serial_init();
    if(OV5640_Init() != 0) {
        Safe_Printf("[ERROR] OV5640 initialization failed!\r\n");
    }
    OV5640_OutSize_Set(4, 0, CAMERA_WIDTH, CAMERA_HEIGHT);
    OV5640_Hue_Set(0); // 这个函数已经移入 ov5640.c
    My_DCMI_Init();
    
    // 5. 耗时最长的网络模块初始化 (此时还没有放狗！)
    ESP8266_Init();
    delay_ms(200);
    
    Safe_Printf("[OK] All Hardware initialized! Starting FreeRTOS...\r\n");
        
    // 6. 创建互斥锁
    Mutex_USART1 = xSemaphoreCreateMutex();
    
    // 7. 创建并启动 FreeRTOS 引导任务
    xTaskCreate((TaskFunction_t )start_task,            
                (const char* )"start_task",          
                (uint16_t       )1024,                   
                (void* )NULL,                  
                (UBaseType_t    )1,                     
                (TaskHandle_t* )&StartTask_Handler);   

    vTaskStartScheduler();
    
    // 正常情况永远不会执行到这里
    while(1) {
        Safe_Printf("[ERROR] Scheduler Failed!\r\n");
        delay_ms(1000);
    }
}

/* ================== FreeRTOS 引导任务 ================== */
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL(); // 进入临界区
    
    // 1. 初始化所有业务层任务
    AI_Decision_Init();
    HMI_Task_Init(); 
    Network_Task_Init();
    Vision_Task_Init();

    // 2. 🚨 终极防御：在系统即将全面运转的最后一刻，放出看门狗！
    // 预分频 64 (prer=4), 重装载 2500 -> 溢出时间 = (64 * 2500) / 32 = 5000ms (5秒)
    // 给 5 秒是为了防止偶尔网络波动导致任务短暂阻塞
    //IWDG_Init(4, 2500); 

    // 3. 自毁引导任务，释放内存
   
    taskEXIT_CRITICAL();  // 退出临界区，系统正式起飞
		vTaskDelete(NULL);
	
}

