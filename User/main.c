// main.c - 主程序 - 战略降级稳定版 (MVP)
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "usart2.h"
#include "usart3.h"
#include "ov5640.h"
#include "dcmi.h"
#include "extsram.h"
#include "jpeg_serial.h"
#include "smoke.h"
#include "task_network.h"
#include "RTC.h"
#include "led.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include  "water_pump.h"
#include "key.h"
#include <stdarg.h>
#include "dht11.h"
#include "ESP8266.h"

// 引入 FreeRTOS 头文件
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

//lwip
#include "malloc.h"


// 引入三大总线和 AI 大脑
#include "ai_decision.h"
#include "task_hmi.h"
#include "task_network.h"
#include "task_vision.h"





// 1. 定义一把全局串口互斥锁
SemaphoreHandle_t Mutex_USART1;

// 2. 打造工业级的安全打印函数

void Safe_Printf(char *format, ...)
{
    // 如果操作系统已经启动，才使用锁机制
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) 
    {
        // 尝试拿锁，只等 1 毫秒。拿不到说明别人在用串口，直接闭嘴退出
        if (xSemaphoreTake(Mutex_USART1, pdMS_TO_TICKS(1)) != pdTRUE) {
            return; 
        }
        
        // 🚨 核心手术：如果底层 DMA（图片传输）正在高速运行，绝对不准插嘴！
        // 直接释放锁并扔掉这条打印信息，保卫图片完整性！
        if ((DMA2_Stream7->CR & DMA_SxCR_EN) != RESET) {
            xSemaphoreGive(Mutex_USART1);
            return; 
        }
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xSemaphoreGive(Mutex_USART1);
    }
}


// 摄像头参数
#define CAMERA_WIDTH  320
#define CAMERA_HEIGHT 240

// 系统参数
#define SENSOR_INTERVAL    5000    // 传感器采集间隔（ms）
#define DATA_SAVE_INTERVAL 1000    // 数据保存间隔（ms）
#define DAILY_REPORT_INTERVAL 86400000  // 日报生成间隔（24小时）
#define CLEANUP_INTERVAL   86400000  // 清理数据间隔（24小时）


 void OV5640_Hue_Set(u8 hue)
{
    switch(hue)
    {
        case 0:  // 0度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x80);
            OV5640_WR_Reg(0x5582, 0x00);
            OV5640_WR_Reg(0x5588, 0x01);
            break;
        case 1:  // +30度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x6F);
            OV5640_WR_Reg(0x5582, 0x40);
            OV5640_WR_Reg(0x5588, 0x01);
            break;
        case 2:  // +60度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x40);
            OV5640_WR_Reg(0x5582, 0x6F);
            OV5640_WR_Reg(0x5588, 0x01);
            break;
        case 3:  // +90度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x00);
            OV5640_WR_Reg(0x5582, 0x80);
            OV5640_WR_Reg(0x5588, 0x01);
            break;
        case 4:  // -30度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x6F);
            OV5640_WR_Reg(0x5582, 0x40);
            OV5640_WR_Reg(0x5588, 0x02);
            break;
        case 5:  // -60度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x40);
            OV5640_WR_Reg(0x5582, 0x6F);
            OV5640_WR_Reg(0x5588, 0x02);
            break;
        case 6:  // -90度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x00);
            OV5640_WR_Reg(0x5582, 0x80);
            OV5640_WR_Reg(0x5588, 0x02);
            break;
        default:
            break;
    }
} 

       

/* ================== FreeRTOS 任务句柄与函数声明 ================== */
TaskHandle_t StartTask_Handler;

void start_task(void *pvParameters);

/* ==================================== */




int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    delay_init(168);
    uart_init(921600);
    
    // 初始化LED
    LED_Init();
    
    // 1. 初始化RTC
    RTC_Init_Custom();
    // 2. 初始化JPEG和串口系统
    jpeg_serial_init();
    
	
	
    Extsram_Init();
    // 3. 初始化摄像头
    if( OV5640_Init() != 0) {
        Safe_Printf("[ERROR] OV5640 initialization failed!\r\n");
    }
    
    OV5640_OutSize_Set(4, 0, CAMERA_WIDTH, CAMERA_HEIGHT);
    OV5640_Hue_Set(0);
        
    //水泵初始化
    WaterPump_Init();
        
  


    // 4. 初始化烟雾传感器
    Smoke_Sensor_Init();
        
    //温湿度初始化
    DHT11_Init();
    KEY_Init();
        
    // 5. 初始化USART2（用于ESP32通信）
    USART2_Init(115200);
    USART2_ConfigInterrupt();
    
    // 6. 初始化ESP8266（WiFi通信）
    USART3_Init(9600);  
    USART3_ConfigInterrupt();  
    ESP8266_Init();
    ESP8266_Report_Init();  
    
    // 8. 初始化DCMI（后初始化DCMI）
    My_DCMI_Init();
    
		
    
    Safe_Printf("[OK] Hardware initialized! Starting FreeRTOS Scheduler...\r\n");
        
    // 创建这把互斥锁
    Mutex_USART1 = xSemaphoreCreateMutex();
    
    // 2. 创建【启动任务】 (分配 256 字节栈，优先级最低设为 1)
    xTaskCreate((TaskFunction_t )start_task,            
                (const char* )"start_task",          
                (uint16_t       )1024,                   
                (void* )NULL,                  
                (UBaseType_t    )1,                     
                (TaskHandle_t* )&StartTask_Handler);   

    // 3. 开启 FreeRTOS 任务调度器！
    vTaskStartScheduler();
    
    while(1) {
        Safe_Printf("[ERROR] FreeRTOS Scheduler failed to start! Check Memory.\r\n");
        delay_ms(1000);
    }
}

/* ================== FreeRTOS 任务实现 ================== */

// 【启动任务】
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL(); // 进入临界区
    
    // 1. 初始化 AI 大脑核心锁
    AI_Decision_Init();

    // 2. 启动 HMI 按键人机交互任务 (假设你已经封装了 HMI_Task_Init)
     HMI_Task_Init(); 

    // 3. 启动 网络处理任务 (黑盒调用)
    Network_Task_Init();

    // 4. 启动 视觉处理任务 (接下来我们要做的)
     Vision_Task_Init();

    vTaskDelete(StartTask_Handler); // 启动完成，自毁过河拆桥
    taskEXIT_CRITICAL();  // 退出临界区
}




