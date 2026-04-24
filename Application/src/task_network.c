/**
 * @file    task_network.c
 * @brief   网络通讯与数据上报任务实现
 * @note    负责协调传感器数据向云端链路的定期推送，并进行下行控制流的高频轮询。
 */
#include "task_network.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266_report.h" 
#include <stdio.h>

/* 任务资源句柄私有化管理 */
static TaskHandle_t NetworkTask_Handler;

/**
 * @brief  网络状态监测与调度控制线程
 * @param  pvParameters 传入参数 (未使用)
 */
static void report_task(void *pvParameters)
{
    ESP8266_Report_Init();
    printf("[System] Network Task Started. TX: 5s, RX Poll: 100ms\r\n");

    while(1)
    {
        /* 1. 触发周期性数据帧聚合与上报 (周期: 5秒) */
        ESP8266_Report_SendSensorData();
        
        /* 2. 分时切片逻辑：利用循环实现细粒度延时，兼顾数据上传与控制流高频监听 */
        for(int i = 0; i < 50; i++)
        {
            ESP8266_Report_PollCommands();
            
            /* 释放时间片，供视觉算法等高计算密度任务运行 */
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

/**
 * @brief  网络传输任务资源配置与初始化
 */
void Network_Task_Init(void)
{
    xTaskCreate((TaskFunction_t )report_task,             
                (const char* )"report_task",           
                (uint16_t       )1024, /* 考虑到 JSON 拼装负荷，预留充分的堆栈空间 */              
                (void* )NULL,                  
                (UBaseType_t    )2,    /* 调整为中等优先级级别 */              
                (TaskHandle_t* )&NetworkTask_Handler); 
}
