/**
 * @file    task_hmi.c
 * @brief   人机交互界面任务实现
 * @note    监听按键动作以支持本地的软切换及异常状态干预。
 */
#include "FreeRTOS.h"
#include "task.h"
#include "key.h"
#include "ai_decision.h"
#include "water_pump.h"
#include "task_hmi.h"

/* 静态句柄分配，限制外部跨域访问 */
static TaskHandle_t HMITask_Handler;

/**
 * @brief  按键事件扫描处理线程
 * @param  pvParameters 传入参数 (未使用)
 */
static void button_scan_task(void *pvParameters)
{
    while(1)
    {
        u8 key_val = KEY_Scan(0);

        /* 模式切换干预 (WKUP按键) */
        if (key_val == WKUP_PRES) {
            SystemMode_t current_mode = AI_Get_System_Mode();
            AI_Set_System_Mode(current_mode == SYS_MODE_AUTO ? SYS_MODE_MANUAL : SYS_MODE_AUTO);
        }

        /* 手动模式下的执行器开闭控制 (KEY1按键) */
        if (AI_Get_System_Mode() == SYS_MODE_MANUAL && key_val == KEY1_PRES) {
            if (WaterPump_GetStatus() == 1) {
                WaterPump_Off(); 
            } else {
                WaterPump_On();  
            }
        }

        /* 系统过载仿真模拟 (KEY0按键短路模拟) */
        if (KEY0_VAL == 0) { 
            AI_Update_Virtual_Current(58.5f);  
        } else {
            AI_Update_Virtual_Current(1.2f);   
        }

        /* 一键紧急接管与阻断 (KEY2按键) */
        if (key_val == KEY2_PRES) {
            AI_Emergency_Override();
        }
        
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}

/**
 * @brief  交互控制任务参数配置与创建
 */
void HMI_Task_Init(void)
{
    xTaskCreate((TaskFunction_t)button_scan_task,             
                (const char* )"task_hmi",           
                (uint16_t      )128,              
                (void* )NULL,                  
                (UBaseType_t   )4,   /* 赋予较高优先级以保障交互响应 */              
                (TaskHandle_t* )&HMITask_Handler); 
}
