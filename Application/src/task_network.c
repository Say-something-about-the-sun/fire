// task_network.c
#include "task_network.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266_report.h" // 网络层只与具体的协议/组件通信
#include <stdio.h>

// 任务句柄设为静态，严禁外部 extern 窃取
static TaskHandle_t NetworkTask_Handler;

// 具体的任务实现（内部函数，无需暴露）
static void report_task(void *pvParameters)
{
    ESP8266_Report_Init();
    printf("[System] Network Task Started. TX: 5s, RX Poll: 100ms\r\n");

    while(1)
    {
        // 1. 每 5 秒执行一次数据上报
        ESP8266_Report_SendSensorData();
        
        // 2. 切碎延时：循环 50 次，每次等 100ms（总计仍是 5 秒）
        for(int i = 0; i < 50; i++)
        {
            // 每 100 毫秒就查一次有没有云端发来的控制指令！
            ESP8266_Report_PollCommands();
            
            // 短暂休眠，让出 CPU 给视觉任务和 AI 任务
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}


// 对外暴露的初始化函数
void Network_Task_Init(void)
{
    // 在这里统一配置网络任务的参数（栈大小，优先级）
    // 防止在 main.c 中出现满天飞的宏定义
    xTaskCreate((TaskFunction_t )report_task,             
                (const char* )"report_task",           
                (uint16_t       )1024, // 网络封包耗费内存，分配 1024 字              
                (void* )NULL,                  
                (UBaseType_t    )2,    // 优先级建议设为中等              
                (TaskHandle_t* )&NetworkTask_Handler); 
}
