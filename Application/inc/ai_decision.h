/**
 * @file    ai_decision.h
 * @brief   系统核心决策中枢头文件
 * @note    定义系统运行模式、核心状态机结构体，并提供决策处理接口。
 */
#ifndef __AI_DECISION_H
#define __AI_DECISION_H

#include "sys.h"
#include "esp8266_report.h" 
#include "fire_detection.h"

/* 系统运行模式枚举 */
typedef enum {
    SYS_MODE_AUTO = 0,
    SYS_MODE_MANUAL = 1
} SystemMode_t;

/* 系统全局状态结构体 (外部模块应通过暴露的 API 访问该状态机) */
typedef struct {
    SystemMode_t mode;           /* 运行模式 (自动/手动) */
    u8 main_power_status;        /* 主电源状态 (1:正常通电, 0:过载跳闸) */
    float virtual_current;       /* 虚拟/真实电流采样值 */
    u8 pump_status;              /* 执行器(水泵)状态 (1:运行, 0:停止) */
    u8 risk_level;               /* 综合危险等级 (0~3) */
    float total_confidence;      /* 综合判定置信度 */
} SystemCoreState_t;

extern SystemCoreState_t g_core_state;

/* 系统初始化与数据同步接口 */
void AI_Decision_Init(void);
void AI_Update_Vision_Result(FireDetectionResult* result);
void AI_Update_Virtual_Current(float current);
void AI_Set_System_Mode(SystemMode_t mode);
SystemMode_t AI_Get_System_Mode(void);

/* 核心决策引擎接口 */
void AI_Fire_Decision_Center(SensorDataPacket* packet);

/* 远程控制与紧急干预接口 */
void AI_Execute_Cloud_Command(const char* cmd);
void AI_Emergency_Override(void);

SystemCoreState_t AI_Get_Core_State_Safe(void);
FireDetectionResult AI_Get_Vision_Result_Safe(void);

#endif /* __AI_DECISION_H */
