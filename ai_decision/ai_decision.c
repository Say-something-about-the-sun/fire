// ai_decision.c
#include "ai_decision.h"
#include "water_pump.h"
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "semphr.h"

// 内部私有全局变量（摒弃 extern）
static SystemCoreState_t g_core_state;
extern FireDetectionResult g_latest_fire_result; // 暂时保留，由 JPEG 任务更新
static FireDetectionResult s_latest_vision_result; // 保存最新的图像检测结果

extern void Safe_Printf(char *format, ...);

// 核心状态保护锁
static SemaphoreHandle_t Mutex_CoreState = NULL;


void AI_Update_Vision_Result(FireDetectionResult* result)
{
    if(Mutex_CoreState != NULL) {
        xSemaphoreTake(Mutex_CoreState, portMAX_DELAY);
        s_latest_vision_result = *result;
        xSemaphoreGive(Mutex_CoreState);
    }
}


void AI_Decision_Init(void)
{
    Mutex_CoreState = xSemaphoreCreateMutex();
    
    // 初始化默认状态
    g_core_state.mode = SYS_MODE_AUTO;
    g_core_state.main_power_status = 1;
    g_core_state.virtual_current = 1.2f;
    g_core_state.pump_status = 0;
    g_core_state.risk_level = 0;
    g_core_state.total_confidence = 100.0f;
}

// 供按键扫描修改系统模式
void AI_Set_System_Mode(SystemMode_t mode)
{
    if(Mutex_CoreState != NULL) {
        xSemaphoreTake(Mutex_CoreState, portMAX_DELAY);
        g_core_state.mode = mode;
        xSemaphoreGive(Mutex_CoreState);
    }
}

SystemMode_t AI_Get_System_Mode(void)
{
    SystemMode_t mode = SYS_MODE_AUTO;
    if(Mutex_CoreState != NULL) {
        xSemaphoreTake(Mutex_CoreState, portMAX_DELAY);
        mode = g_core_state.mode;
        xSemaphoreGive(Mutex_CoreState);
    }
    return mode;
}

void AI_Update_Virtual_Current(float current)
{
    if(Mutex_CoreState != NULL) {
        xSemaphoreTake(Mutex_CoreState, portMAX_DELAY);
        g_core_state.virtual_current = current;
        xSemaphoreGive(Mutex_CoreState);
    }
}

// 执行云端下发的命令（解耦：网络层只负责传递字符串，由这里负责控制硬件）
void AI_Execute_Cloud_Command(const char* cmd)
{
    if(Mutex_CoreState != NULL) xSemaphoreTake(Mutex_CoreState, portMAX_DELAY);
    
    if (strstr(cmd, "CMD:PUMP:1") != NULL) {
        g_core_state.mode = SYS_MODE_MANUAL; 
        WaterPump_On();
        g_core_state.pump_status = 1;
    }
    else if (strstr(cmd, "CMD:PUMP:0") != NULL) {
        g_core_state.mode = SYS_MODE_MANUAL; 
        WaterPump_Off();
        g_core_state.pump_status = 0;
    }
    else if (strstr(cmd, "CMD:MODE:0") != NULL) {
        g_core_state.mode = SYS_MODE_AUTO;
        Safe_Printf("=> 已恢复 AI 自动托管！\r\n");
    }
    else if (strstr(cmd, "CMD:MODE:1") != NULL) {
        g_core_state.mode = SYS_MODE_MANUAL;
        Safe_Printf("=> 已切入人工接管模式！\r\n");
    }
    
    if(Mutex_CoreState != NULL) xSemaphoreGive(Mutex_CoreState);
}

// 紧急超驰接管（按键2触发）
void AI_Emergency_Override(void)
{
    if(Mutex_CoreState != NULL) xSemaphoreTake(Mutex_CoreState, portMAX_DELAY);
    
    g_core_state.mode = SYS_MODE_MANUAL;
    g_core_state.virtual_current = 15.0f;
    WaterPump_On();
    g_core_state.pump_status = 1;
    Safe_Printf("\r\n>>> [EMERGENCY] Manual Override Activated! Pump ON! <<<\r\n");
    
    if(Mutex_CoreState != NULL) xSemaphoreGive(Mutex_CoreState);
}

// 原 esp8266_report.c 中的业务中枢，现在回归 AI 层
void AI_Fire_Decision_Center(SensorDataPacket* packet)
{
    if(Mutex_CoreState != NULL) xSemaphoreTake(Mutex_CoreState, portMAX_DELAY);

    // 1. 同步输入数据
		packet->image_fire_detected = s_latest_vision_result.fire_detected;
    packet->image_confidence = s_latest_vision_result.confidence;
    packet->fire_area = s_latest_vision_result.fire_area;
    packet->fire_center_x = s_latest_vision_result.fire_center_x;
    packet->fire_center_y = s_latest_vision_result.fire_center_y;
    packet->flicker_detected = s_latest_vision_result.flicker_detected;
	
	
    packet->virtual_current = g_core_state.virtual_current;
    u8 is_fire_real = packet->image_fire_detected || packet->fire_detected || packet->flame_do == 1;

    // 🚨 绝对优先级：严重过载（模拟漏电/短路），无视任何模式，立刻断电停泵！
    if (g_core_state.virtual_current > 10.0) 
    {
        g_core_state.risk_level = 2; // 高危预警
        g_core_state.total_confidence = 100.0f;
        g_core_state.main_power_status = 0; // 全局跳闸标志
        
        WaterPump_Off(); // 强制停止物理硬件！
        g_core_state.pump_status = 0;
        
        // 强制切为手动模式，防止系统在短路状态下反复自动重启
        g_core_state.mode = SYS_MODE_MANUAL; 
    }
    else 
    {
        // 正常电流状态下，主电网恢复供电标志
        g_core_state.main_power_status = 1;

        if (is_fire_real) 
        {
            // 🔴 Level 3: 确认起火
            g_core_state.risk_level = 3; 
            g_core_state.total_confidence = packet->image_fire_detected ? packet->image_confidence : 85.0f;
            
            // 🚨 权限判定：只有在【自动模式】下，AI 才有权开启水泵
            if (g_core_state.mode == SYS_MODE_AUTO) {
                if (g_core_state.pump_status == 0) {
                    WaterPump_On();
                    g_core_state.pump_status = 1;
                }
            }
        } 
        else 
        {
            // 🟢 Level 1/0: 没有火灾 (可能只有烟雾)
            g_core_state.risk_level = (packet->smoke_detected) ? 1 : 0;
            g_core_state.total_confidence = (packet->smoke_detected) ? 60.0f : 100.0f;

            // 🚨 收尾判定：只有在【自动模式】下，AI 才会在火灭后自动关闭水泵
            if (g_core_state.mode == SYS_MODE_AUTO) {
                if (g_core_state.pump_status == 1) {
                    WaterPump_Off();
                    g_core_state.pump_status = 0;
                }
            }
        }
    }

    // 2. 实时兜底：无论 AI 怎么决定，最终读取一次底层引脚的真实状态
    g_core_state.pump_status = WaterPump_GetStatus(); 

    // 3. 将最终决策状态回填给打包器
    packet->risk_level = g_core_state.risk_level;
    packet->confidence = g_core_state.total_confidence;
    packet->pump_status = g_core_state.pump_status; 
    packet->system_mode = g_core_state.mode;
    packet->main_power_status = g_core_state.main_power_status;

    if(Mutex_CoreState != NULL) xSemaphoreGive(Mutex_CoreState);
}

