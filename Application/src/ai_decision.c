/**
 * @file    ai_decision.c
 * @brief   系统核心决策中枢实现
 * @note    聚合多维传感器数据，利用施密特触发器逻辑(迟滞防抖)进行工业级火灾判定，
 * 并负责对云端下发指令及本地紧急中断信号进行解析与执行。
 */
#include "ai_decision.h"
#include "water_pump.h"
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "semphr.h"

/* 内部私有状态存储 */
static SystemCoreState_t g_core_state;
extern FireDetectionResult g_latest_fire_result;   /* 预留接口，由视觉任务更新 */
static FireDetectionResult s_latest_vision_result; /* 图像检测结果缓存 */

extern void Safe_Printf(char *format, ...);

SystemCoreState_t g_core_state;

/* 环境基线与校准参数 */
static float base_temp = -100.0f; 
static float base_smoke = -1.0f;
static u16 calib_samples = 0;
#define CALIB_MAX_SAMPLES 6  /* 前置校准周期数 (假设5秒采集一次，约30秒完成校准) */

/* ==============================================================================
 * 毕设演示级 AI 决策参数配置字典
 * 调整以下宏定义以掌控系统的灵敏度与鲁棒性
 * ============================================================================== */

/* --- 1. 系统基线与校准配置 --- */
#define CFG_CALIB_SAMPLES       10      /* 开机校准迭代次数 (约5秒完成校准) */
#define CFG_ENV_TEMP_NORMAL     40.0f   /* 正常环境温度上限 (超过此值不计入动态基线) */
#define CFG_ENV_SMOKE_NORMAL    100.0f  /* 正常环境烟雾上限 */
#define CFG_DRIFT_LIMIT         2.0f    /* 单次允许的最大温度漂移量 (防止缓慢环境变化导致的适应性误判) */

/* --- 2. 绝对危险红线阈值 --- */
#define CFG_CRITICAL_TEMP       60.0f   /* 温度物理红线 (触发强制报警) */
#define CFG_CRITICAL_SMOKE      300.0f  /* 烟雾物理红线 */

/* --- 3. 突变(Delta)特征判定阈值 --- */
#define CFG_DELTA_TEMP_HIGH     15.0f   /* 温度高突变阈值 */
#define CFG_DELTA_TEMP_LOW      8.0f    /* 温度低突变阈值 */
#define CFG_DELTA_SMOKE_HIGH    100.0f  /* 烟雾高突变阈值 */
#define CFG_DELTA_SMOKE_LOW     40.0f   /* 烟雾低突变阈值 */

/* --- 4. 多维特征打分权重配置 --- */
#define SCORE_VISION_COLOR      70.0f   /* 静态红色特征得分 (低于报警线，过滤单色物体误报) */
#define SCORE_VISION_FLICKER    20.0f   /* 动态频闪特征附加得分 */
#define SCORE_TEMP_CRITICAL     60.0f   /* 突破温度红线得分 */
#define SCORE_TEMP_HIGH         25.0f   /* 温度高突变特征得分 */
#define SCORE_TEMP_LOW          15.0f   /* 温度低突变特征得分 */
#define SCORE_SMOKE_CRITICAL    50.0f   /* 突破烟雾红线得分 */
#define SCORE_SMOKE_HIGH        30.0f   /* 烟雾高突变特征得分 */
#define SCORE_SMOKE_LOW         15.0f   /* 烟雾低突变特征得分 */
#define SCORE_ESP32_FLAME       25.0f   /* 辅助红外传感器报警得分 */

/* --- 5. 决策触发与迟滞防抖参数 --- */
#define CFG_SCORE_TRIGGER       80.0f   /* 综合风险报警触发线 */
#define CFG_SCORE_SAFE          60.0f   /* 风险解除与设备复位安全线 */
#define CFG_HYST_TRIGGER_CNT    1       /* 触发阈值连续命中要求 (降低以提高演示响应速度) */
#define CFG_HYST_SAFE_CNT       1       /* 恢复阈值连续命中要求 (防止执行器频繁开合) */
/* ============================================================================== */

/* 核心状态机资源保护锁 */
static SemaphoreHandle_t Mutex_CoreState = NULL;

/**
 * @brief  更新视觉特征评估结果
 */
void AI_Update_Vision_Result(FireDetectionResult* result)
{
    if(Mutex_CoreState != NULL) {
        xSemaphoreTake(Mutex_CoreState, portMAX_DELAY);
        s_latest_vision_result = *result;
        xSemaphoreGive(Mutex_CoreState);
    }
}

/**
 * @brief  决策中枢及同步锁初始化
 */
void AI_Decision_Init(void)
{
    Mutex_CoreState = xSemaphoreCreateMutex();
    
    g_core_state.mode = SYS_MODE_AUTO;
    g_core_state.main_power_status = 1;
    g_core_state.virtual_current = 1.2f;
    g_core_state.pump_status = 0;
    g_core_state.risk_level = 0;
    g_core_state.total_confidence = 0.0f;
}

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

/**
 * @brief  硬件层紧急人工介入处理
 * @note   越过 AI 逻辑，强制开启执行器并切换系统至手动模式。
 */
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

/*
// 原 esp8266_report.c 中的业务中枢历史代码（保留供对比参考）
void AI_Fire_Decision_Center(SensorDataPacket* packet)
{
    // [原历史版本代码保留]
    // ...
}
*/

/**
 * @brief  火灾综合决策核心算法
 * @param  packet 传感器聚合数据包
 * @note   包含动态基线校准、多维特征融合打分以及带迟滞效应的执行器调度。
 */
void AI_Fire_Decision_Center(SensorDataPacket* packet)
{
    /* 0. 获取状态锁，确保多任务数据一致性 */
    if(Mutex_CoreState != NULL) xSemaphoreTake(Mutex_CoreState, portMAX_DELAY);

    /* 状态机静态变量 (环境基线与迟滞防抖) */
    static float base_temp = 25.0f;
    static float base_smoke = 0.0f;
    static u16 calib_samples = 0;
    static u8 over_threshold_counter = 0; 
    static u8 safe_threshold_counter = 0; 
    
    float total_score = 0.0f;
    float delta_temp = 0.0f;
    float delta_smoke = 0.0f;
    
    /* 1. 同步视觉特征与系统监测数据至数据包 */
    packet->image_fire_detected = s_latest_vision_result.fire_detected;
    packet->image_confidence    = s_latest_vision_result.confidence;
    packet->fire_area           = s_latest_vision_result.fire_area;
    packet->fire_center_x       = s_latest_vision_result.fire_center_x;
    packet->fire_center_y       = s_latest_vision_result.fire_center_y;
    packet->flicker_detected    = s_latest_vision_result.flicker_detected;
    packet->virtual_current     = g_core_state.virtual_current;

    /* ==========================================
     * 2. 硬件保护高优先级阻断：严重过载处理
     * ========================================== */
    if (g_core_state.virtual_current > 10.0f) 
    {
        g_core_state.risk_level = 2;         
        g_core_state.total_confidence = 100.0f;
        g_core_state.main_power_status = 0;  /* 模拟主电网跳闸保护 */
        
        WaterPump_Off();                     
        g_core_state.pump_status = 0;
        
        g_core_state.mode = SYS_MODE_MANUAL; 
        Safe_Printf("[CRITICAL] Current Overload! Main Power Tripped.\r\n");
        
        /* 发生硬性故障跳闸，越过环境算法直接进行封包回传 */
        goto PACK_AND_EXIT; 
    }
    else 
    {
        g_core_state.main_power_status = 1;  
    }

    /* ==========================================
     * 3. 安全基线初始化校准
     * ========================================== */
    if (calib_samples < CFG_CALIB_SAMPLES) {
        if (packet->temperature > 0 && packet->temperature < CFG_ENV_TEMP_NORMAL && packet->smoke_ppm < CFG_ENV_SMOKE_NORMAL) {
            if (calib_samples == 0) {
                base_temp = packet->temperature; 
                base_smoke = packet->smoke_ppm;
            } else {
                base_temp = (base_temp * 0.9f) + (packet->temperature * 0.1f);
                base_smoke = (base_smoke * 0.9f) + (packet->smoke_ppm * 0.1f);
            }
            calib_samples++;
        }
        
        /* 若在校准期内发生极端突变，强制中断校准并执行安全评估 */
        if (packet->temperature >= CFG_CRITICAL_TEMP || packet->smoke_ppm >= CFG_CRITICAL_SMOKE || packet->image_fire_detected) {
            goto RISK_EVALUATION; 
        }
        
        g_core_state.risk_level = 0; 
        g_core_state.total_confidence = 0.0f; 
        
        /* 若在校准期间干扰物撤离，执行器复位归位 */
        if (g_core_state.mode == SYS_MODE_AUTO && g_core_state.pump_status == 1) {
            g_core_state.pump_status = 0;
            WaterPump_Off();
        }
        goto PACK_AND_EXIT; 
    }

    /* ==========================================
     * 4. 环境动态追踪漂移修正
     * ========================================== */
    if (packet->temperature < (CFG_ENV_TEMP_NORMAL - 5.0f) && packet->smoke_ppm < (CFG_ENV_SMOKE_NORMAL - 50.0f)) {
        float temp_diff = packet->temperature - base_temp;
        if (temp_diff > -CFG_DRIFT_LIMIT && temp_diff < CFG_DRIFT_LIMIT) { 
            base_temp = (base_temp * 0.999f) + (packet->temperature * 0.001f);
        }
    }

RISK_EVALUATION:
    /* ==========================================
     * 5. 多维特征加权打分引擎
     * ========================================== */
    /* 维度 A: 视觉算法特征 */
    if (packet->image_fire_detected) {
        total_score += SCORE_VISION_COLOR;  
        if (packet->flicker_detected) {
            total_score += SCORE_VISION_FLICKER; 
        }
    }
    
    /* 维度 B: 外部辅助红外传感器特征 */
    if (packet->fire_detected || packet->flame_do == 1) {
        total_score += SCORE_ESP32_FLAME; 
    }

    /* 维度 C: 环境温度参量 */
    delta_temp = packet->temperature - base_temp;
    if (packet->temperature >= CFG_CRITICAL_TEMP) total_score += SCORE_TEMP_CRITICAL;      
    else if (delta_temp > CFG_DELTA_TEMP_HIGH)    total_score += SCORE_TEMP_HIGH;          
    else if (delta_temp > CFG_DELTA_TEMP_LOW)     total_score += SCORE_TEMP_LOW;            

    /* 维度 D: 气溶胶浓度(烟雾)参量 */
    delta_smoke = packet->smoke_ppm - base_smoke;
    if (packet->smoke_ppm >= CFG_CRITICAL_SMOKE)  total_score += SCORE_SMOKE_CRITICAL;       
    else if (delta_smoke > CFG_DELTA_SMOKE_HIGH)  total_score += SCORE_SMOKE_HIGH;         
    else if (delta_smoke > CFG_DELTA_SMOKE_LOW)   total_score += SCORE_SMOKE_LOW;          

    g_core_state.total_confidence = total_score;

    /* ==========================================
     * 6. 施密特工业迟滞控制逻辑
     * ========================================== */
    if (total_score >= CFG_SCORE_TRIGGER) {
        safe_threshold_counter = 0; 
        if (over_threshold_counter < CFG_HYST_TRIGGER_CNT) { 
            over_threshold_counter++;
        } else {
            g_core_state.risk_level = 3; 
            if (g_core_state.mode == SYS_MODE_AUTO && g_core_state.pump_status == 0) {
                g_core_state.pump_status = 1;
                WaterPump_On();
                Safe_Printf("[AI] Risk CONFIRMED! Auto-triggering pump.\r\n");
            }
        }
    } 
    else if (total_score < CFG_SCORE_SAFE) {
        over_threshold_counter = 0; 
        if (safe_threshold_counter < CFG_HYST_SAFE_CNT) { 
            safe_threshold_counter++;
        } else {
            g_core_state.risk_level = 0; 
            if (g_core_state.mode == SYS_MODE_AUTO && g_core_state.pump_status == 1) {
                g_core_state.pump_status = 0;
                WaterPump_Off();
                Safe_Printf("[AI] Environment stable. Auto-stopping pump.\r\n");
            }
        }
    }
    else {
        /* 落入中间迟滞隔离带，保持设备现状，避免继电器频繁动作 */
        g_core_state.risk_level = 2; 
    }

PACK_AND_EXIT:
    /* ==========================================
     * 7. 物理层状态回读兜底与数据流解锁
     * ========================================== */
    g_core_state.pump_status = WaterPump_GetStatus(); 

    packet->risk_level        = g_core_state.risk_level;
    packet->confidence        = g_core_state.total_confidence;
    packet->pump_status       = g_core_state.pump_status; 
    packet->system_mode       = g_core_state.mode;
    packet->main_power_status = g_core_state.main_power_status;

    if(Mutex_CoreState != NULL) xSemaphoreGive(Mutex_CoreState);
}

/**
 * @brief  云端下行指令解析执行接口
 * @param  json_str 下行字符串报文
 */
void AI_Execute_Cloud_Command(const char* json_str)
{
    /* 匹配开启水泵强行干预控制字 */
    if (strstr(json_str, "CMD:PUMP:1") != NULL) 
    {
        g_core_state.mode = SYS_MODE_MANUAL; 
        g_core_state.pump_status = 1;
        WaterPump_On();
        Safe_Printf("[Cloud CMD] MANUAL OVERRIDE: PUMP ON!\r\n");
    }
    /* 匹配关闭水泵强行干预控制字 */
    else if (strstr(json_str, "CMD:PUMP:0") != NULL) 
    {
        g_core_state.mode = SYS_MODE_MANUAL; 
        g_core_state.pump_status = 0;
        WaterPump_Off();
        Safe_Printf("[Cloud CMD] MANUAL OVERRIDE: PUMP OFF!\r\n");
    }
    /* 匹配系统模式恢复指令 */
    else if (strstr(json_str, "CMD:MODE:0") != NULL || strstr(json_str, "\"system_mode\":0") != NULL) 
    {
        g_core_state.mode = SYS_MODE_AUTO;   
        g_core_state.pump_status = 0; 
        WaterPump_Off();
        Safe_Printf("[Cloud CMD] AI AUTO MODE RESTORED!\r\n");
    }
}
