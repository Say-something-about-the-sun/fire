// ai_decision.h
#ifndef __AI_DECISION_H
#define __AI_DECISION_H

#include "sys.h"
#include "esp8266_report.h" // 借用你的 SensorDataPacket 定义
#include "fire_detection.h"





// 系统运行模式枚举
typedef enum {
    SYS_MODE_AUTO = 0,
    SYS_MODE_MANUAL = 1
} SystemMode_t;

// 系统全局状态黑匣子（外部不可见，只能通过API访问）
typedef struct {
    SystemMode_t mode;           // 自动/手动
    u8 main_power_status;        // 1:通电, 0:跳闸
    float virtual_current;       // 虚拟电流
    u8 pump_status;              // 水泵状态 1:开, 0:关
    u8 risk_level;               // 风险等级 0-3
    float total_confidence;      // 综合置信度
} SystemCoreState_t;



	extern SystemCoreState_t g_core_state;

// 1. 系统初始化
void AI_Decision_Init(void);

// 2. 数据更新接口（供外设任务调用）
void AI_Update_Vision_Result(FireDetectionResult* result);
void AI_Update_Virtual_Current(float current);
void AI_Set_System_Mode(SystemMode_t mode);
SystemMode_t AI_Get_System_Mode(void);

// 3. 核心决策中枢（供数据汇聚后调用）
void AI_Fire_Decision_Center(SensorDataPacket* packet);

// 4. 云端与紧急指令执行接口
void AI_Execute_Cloud_Command(const char* cmd);
void AI_Emergency_Override(void);



SystemCoreState_t AI_Get_Core_State_Safe(void);
FireDetectionResult AI_Get_Vision_Result_Safe(void);
#endif
