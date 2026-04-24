/**
 * @file    main.c
 * @brief   系统主程序入口
 * @note    负责硬件抽象层(BSP)的初始化，以及 FreeRTOS 系统调度器的启动。
 */
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "usart2.h"
#include "usart3.h"

/* 硬件驱动层 (BSP) */
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

/* 业务组件层 */
#include "jpeg_serial.h"

/* RTOS与核心调度层 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ai_decision.h"
#include "task_hmi.h"
#include "task_network.h"
#include "task_vision.h"

#include <stdio.h>
#include <stdarg.h>

/* 全局互斥锁与标志位 */
SemaphoreHandle_t Mutex_USART1;
extern volatile u8 g_uart_is_sending_image; 

/**
 * @brief  线程安全的日志打印输出
 * @param  format 格式化字符串
 */
void Safe_Printf(char *format, ...)
{
    
    return;
}

/* 摄像头参数宏 */
#define CAMERA_WIDTH  320
#define CAMERA_HEIGHT 240

/* 任务句柄与声明 */
TaskHandle_t StartTask_Handler;
void start_task(void *pvParameters);

/**
 * @brief  主函数入口
 */
int main(void)
{
    /* 1. 基础系统时钟与外设初始化 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    delay_init(168);
    uart_init(921600);
    
    /* 2. 硬件设备初始化 */
    LED_Init();
    KEY_Init();
    Extsram_Init();
    RTC_Init_Custom();
    DHT11_Init();
    Smoke_Sensor_Init();
    WaterPump_Init();
    
    /* 3. 通信总线初始化 */
    USART2_Init(115200);
    USART2_ConfigInterrupt();
    USART3_Init(9600);  
    USART3_ConfigInterrupt();  
    
    /* 4. 摄像头与 JPEG 编码系统初始化 */
    jpeg_serial_init();
    if(OV5640_Init() != 0) {
        Safe_Printf("[ERROR] OV5640 initialization failed!\r\n");
    }
    OV5640_OutSize_Set(4, 0, CAMERA_WIDTH, CAMERA_HEIGHT);
    OV5640_Hue_Set(0); 
    My_DCMI_Init();
    
    /* 5. 网络模块初始化 (暂不启动看门狗以预留初始化时间) */
    ESP8266_Init();
    delay_ms(200);
    
    Safe_Printf("[OK] All Hardware initialized! Starting FreeRTOS...\r\n");
        
    /* 6. 创建系统级互斥锁 */
    Mutex_USART1 = xSemaphoreCreateMutex();
    
    /* 7. 创建并启动 FreeRTOS 引导任务 */
    xTaskCreate((TaskFunction_t )start_task,            
                (const char* )"start_task",          
                (uint16_t       )1024,                   
                (void* )NULL,                  
                (UBaseType_t    )1,                     
                (TaskHandle_t* )&StartTask_Handler);   

    vTaskStartScheduler();
    
    /* 系统调度器启动失败时的异常处理 */
    while(1) {
        Safe_Printf("[ERROR] Scheduler Failed!\r\n");
        delay_ms(1000);
    }
}

/**
 * @brief  FreeRTOS 系统引导任务
 * @param  pvParameters 传入参数 (未使用)
 * @note   负责初始化应用层任务，并在最后阶段启动系统安全机制。
 */
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL(); /* 进入临界区，防止初始化过程被中断 */
    
    /*  初始化各业务层任务 */
    AI_Decision_Init();
    HMI_Task_Init(); 
    Network_Task_Init();
    Vision_Task_Init();

    

    /*  退出临界区，启动系统任务调度 */
    taskEXIT_CRITICAL();  
    
    /* 自毁引导任务，释放系统资源 */
    vTaskDelete(NULL);
}
