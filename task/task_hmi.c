#include "FreeRTOS.h"
#include "task.h"
#include "key.h"
#include "ai_decision.h"
#include "water_pump.h"


// 静态句柄，拒绝外部 extern
static TaskHandle_t HMITask_Handler;


static void button_scan_task(void *pvParameters)
{
    while(1)
    {
        u8 key_val = KEY_Scan(0);

        if (key_val == WKUP_PRES) {
            // 切换模式
            SystemMode_t current_mode = AI_Get_System_Mode();
            AI_Set_System_Mode(current_mode == SYS_MODE_AUTO ? SYS_MODE_MANUAL : SYS_MODE_AUTO);
        }

        if (AI_Get_System_Mode() == SYS_MODE_MANUAL && key_val == KEY1_PRES) {
            if (WaterPump_GetStatus() == 1) {
                WaterPump_Off(); 
            } else {
                WaterPump_On();  
            }
        }

        // 模拟电流短路
        if (KEY0_VAL == 0) { 
            AI_Update_Virtual_Current(58.5f);  
        } else {
            AI_Update_Virtual_Current(1.2f);   
        }

        // 紧急接管
        if (key_val == KEY2_PRES) {
            AI_Emergency_Override();
        }
        
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}

// 统一配置任务参数
void HMI_Task_Init(void)
{
    xTaskCreate((TaskFunction_t)button_scan_task,             
                (const char* )"task_hmi",           
                (uint16_t      )128,              
                (void* )NULL,                  
                (UBaseType_t   )4,   // 按键响应需要高优先级              
                (TaskHandle_t* )&HMITask_Handler); 
}

