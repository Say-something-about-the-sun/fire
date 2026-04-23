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

SystemCoreState_t g_core_state;

static float base_temp = -100.0f; 
static float base_smoke = -1.0f;
static u16 calib_samples = 0;
#define CALIB_MAX_SAMPLES 6  // 假设 5 秒采集一次，6次即前 30 秒为校准期



// --- 1. 系统基线与校准配置 ---
#define CFG_CALIB_SAMPLES       10      // 开机校准次数 (设为10，约5秒完成校准，方便快速演示)
#define CFG_ENV_TEMP_NORMAL     40.0f   // 正常环境温度上限 (超过此值不计入环境基线)
#define CFG_ENV_SMOKE_NORMAL    100.0f  // 正常环境烟雾上限
#define CFG_DRIFT_LIMIT         2.0f    // 单次允许的环境温度漂移最大值 (防温水煮青蛙)

// --- 2. 绝对危险红线阈值 ---
#define CFG_CRITICAL_TEMP       60.0f   // 温度红线 (打火机直烤极易触发)
#define CFG_CRITICAL_SMOKE      300.0f  // 烟雾红线 

// --- 3. 突变(Delta)判定阈值 ---
#define CFG_DELTA_TEMP_HIGH     15.0f   // 温度突变极高阀值
#define CFG_DELTA_TEMP_LOW      8.0f    // 温度突变疑似阀值
#define CFG_DELTA_SMOKE_HIGH    100.0f  // 烟雾突变极高阀值
#define CFG_DELTA_SMOKE_LOW     40.0f   // 烟雾突变疑似阀值

// --- 4. 🥇 核心打分权重配置 (满分不设限，触发线见下方) ---
#define SCORE_VISION_COLOR      70.0f   // [关键] 纯静态红色物体得分 (必须低于触发线，防误报)
#define SCORE_VISION_FLICKER    20.0f   // 动态闪烁特征附加分 (65+20=85，满足报警！)
#define SCORE_TEMP_CRITICAL     60.0f   // 温度超红线得分
#define SCORE_TEMP_HIGH         40.0f   // 温度突变极高得分
#define SCORE_TEMP_LOW          15.0f   // 温度突变疑似得分
#define SCORE_SMOKE_CRITICAL    50.0f   // 烟雾超红线得分
#define SCORE_SMOKE_HIGH        30.0f   // 烟雾突变极高得分
#define SCORE_SMOKE_LOW         15.0f   // 烟雾突变疑似得分

// --- 5. ⏱️ 决策触发与工业迟滞防抖 ---
#define CFG_SCORE_TRIGGER       80.0f   // 🚨 最终报警喷水触发线！
#define CFG_SCORE_SAFE          60.0f   // 🕊️ 恢复安全的解除线！
#define CFG_HYST_TRIGGER_CNT    1       // 连续超限几次才喷水 (设为2，即约1秒，演示反应极快且防单次毛刺)
#define CFG_HYST_SAFE_CNT       1       // 连续安全几次才停水 (设为4，即约2秒，防止水泵抽搐)
// ==============================================================================

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
    g_core_state.total_confidence = 0.0f;
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


/*
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
    if (g_core_state.virtual_current > 10.0f) 
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
*/



void AI_Fire_Decision_Center(SensorDataPacket* packet)
{
    // ==========================================
    // 0. 获取状态锁 (极其重要，防多任务数据踩踏)
    // ==========================================
    if(Mutex_CoreState != NULL) xSemaphoreTake(Mutex_CoreState, portMAX_DELAY);

    // 状态机静态变量 (环境基线与防抖)
    static float base_temp = 25.0f;
    static float base_smoke = 0.0f;
    static u16 calib_samples = 0;
    static u8 over_threshold_counter = 0; 
    static u8 safe_threshold_counter = 0; 
    
    float total_score = 0.0f;

    // 1. 同步视觉数据与电流数据到包
    packet->image_fire_detected = s_latest_vision_result.fire_detected;
    packet->image_confidence    = s_latest_vision_result.confidence;
    packet->fire_area           = s_latest_vision_result.fire_area;
    packet->fire_center_x       = s_latest_vision_result.fire_center_x;
    packet->fire_center_y       = s_latest_vision_result.fire_center_y;
    packet->flicker_detected    = s_latest_vision_result.flicker_detected;
    packet->virtual_current     = g_core_state.virtual_current;

    // ==========================================
    // 2. 🚨 绝对优先级：严重过载一票否决
    // ==========================================
    if (g_core_state.virtual_current > 10.0f) 
    {
        g_core_state.risk_level = 2;         // 漏电/短路高危
        g_core_state.total_confidence = 100.0f;
        g_core_state.main_power_status = 0;  // 主电网跳闸
        
        WaterPump_Off();                     // 物理断电防二次灾害
        g_core_state.pump_status = 0;
        
        g_core_state.mode = SYS_MODE_MANUAL; // 强切手动，需要人工排查后恢复
        Safe_Printf("[CRITICAL] Current Overload! Main Power Tripped.\r\n");
        
        // 既然已经短路跳闸，直接跳过所有火灾 AI 检测，进入数据封包
        goto PACK_AND_EXIT; 
    }
    else 
    {
        g_core_state.main_power_status = 1;  // 电流正常，供电恢复
    }

    // ==========================================
    // 3. 安全基线校准 (仅在前 N 次采样)
    // ==========================================
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
        
        // 即使在校准期，如果发生极端突变，立刻中断校准去打分
        if (packet->temperature >= CFG_CRITICAL_TEMP || packet->smoke_ppm >= CFG_CRITICAL_SMOKE || packet->image_fire_detected) {
            goto RISK_EVALUATION; 
        }
        
        g_core_state.risk_level = 0; 
        g_core_state.total_confidence = 0.0f; // 校准期不乱报警
        
        // 如果在校准期间人手移开了干扰物，需归位水泵
        if (g_core_state.mode == SYS_MODE_AUTO && g_core_state.pump_status == 1) {
            g_core_state.pump_status = 0;
            WaterPump_Off();
        }
        goto PACK_AND_EXIT; 
    }

    // ==========================================
    // 4. 环境缓慢漂移修正 (防温水煮青蛙)
    // ==========================================
    if (packet->temperature < (CFG_ENV_TEMP_NORMAL - 5.0f) && packet->smoke_ppm < (CFG_ENV_SMOKE_NORMAL - 50.0f)) {
        float temp_diff = packet->temperature - base_temp;
        if (temp_diff > -CFG_DRIFT_LIMIT && temp_diff < CFG_DRIFT_LIMIT) { 
            base_temp = (base_temp * 0.999f) + (packet->temperature * 0.001f);
        }
    }

RISK_EVALUATION:
    // ==========================================
    // 5. 🎯 多维加权打分系统
    // ==========================================
    // 维度 A: 视觉 AI
    if (packet->image_fire_detected) {
        total_score += SCORE_VISION_COLOR;  
        if (packet->flicker_detected) {
            total_score += SCORE_VISION_FLICKER; 
        }
    }
    
    // 维度 B: 外部物理传感器 (整合你的原版逻辑)
    if (packet->fire_detected || packet->flame_do == 1) {
        // 如果外部红外/ESP32直接报告有火，赋予极高权重
        total_score += 80.0f; 
    }

    // 维度 C: 温度
    float delta_temp = packet->temperature - base_temp;
    if (packet->temperature >= CFG_CRITICAL_TEMP) total_score += SCORE_TEMP_CRITICAL;      
    else if (delta_temp > CFG_DELTA_TEMP_HIGH)    total_score += SCORE_TEMP_HIGH;          
    else if (delta_temp > CFG_DELTA_TEMP_LOW)     total_score += SCORE_TEMP_LOW;            

    // 维度 D: 烟雾
    float delta_smoke = packet->smoke_ppm - base_smoke;
    if (packet->smoke_ppm >= CFG_CRITICAL_SMOKE)  total_score += SCORE_SMOKE_CRITICAL;       
    else if (delta_smoke > CFG_DELTA_SMOKE_HIGH)  total_score += SCORE_SMOKE_HIGH;         
    else if (delta_smoke > CFG_DELTA_SMOKE_LOW)   total_score += SCORE_SMOKE_LOW;          

    g_core_state.total_confidence = total_score;

    // ==========================================
    // 6. 工业级迟滞决策器
    // ==========================================
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
        g_core_state.risk_level = 2; // 中间迟滞带，维持设备现状
    }

PACK_AND_EXIT:
    // ==========================================
    // 7. 兜底回填与解锁 (原版的精华)
    // ==========================================
    // 无论上面发生了什么，真实读取一次硬件状态作为最终真相
    g_core_state.pump_status = WaterPump_GetStatus(); 

    // 把大脑里计算好的最终结果，塞给网络层的数据包
    packet->risk_level        = g_core_state.risk_level;
    packet->confidence        = g_core_state.total_confidence;
    packet->pump_status       = g_core_state.pump_status; 
    packet->system_mode       = g_core_state.mode;
    packet->main_power_status = g_core_state.main_power_status;

    // 释放状态锁
    if(Mutex_CoreState != NULL) xSemaphoreGive(Mutex_CoreState);
}




// ==================================================================
// 🌩️ 云端指令执行器（精准匹配物理探针抓取的真实数据）
// ==================================================================
void AI_Execute_Cloud_Command(const char* json_str)
{
    // 1. 拦截【强制开启水泵】 -> 对应你抓到的 CMD:PUMP:1
    if (strstr(json_str, "CMD:PUMP:1") != NULL) 
    {
        g_core_state.mode = SYS_MODE_MANUAL; 
        g_core_state.pump_status = 1;
        WaterPump_On();
        Safe_Printf("[Cloud CMD] 🚨 MANUAL OVERRIDE: PUMP ON!\r\n");
    }
    // 2. 拦截【强制关闭水泵】 -> 对应你抓到的 CMD:PUMP:0
    else if (strstr(json_str, "CMD:PUMP:0") != NULL) 
    {
        g_core_state.mode = SYS_MODE_MANUAL; 
        g_core_state.pump_status = 0;
        WaterPump_Off();
        Safe_Printf("[Cloud CMD] 🚨 MANUAL OVERRIDE: PUMP OFF!\r\n");
    }
    
    // 3. 拦截【恢复AI托管】
    // (注意：如果你不知道恢复AI会发什么，可以点一下App上的恢复按钮，看看串口打印什么，然后填到这里)
    // 这里预留常见的几种格式防弹：
    else if (strstr(json_str, "CMD:MODE:0") != NULL || strstr(json_str, "\"system_mode\":0") != NULL) 
    {
        g_core_state.mode = SYS_MODE_AUTO;   
        g_core_state.pump_status = 0; // 交还控制权后默认关水，等待AI下一秒判断
        WaterPump_Off();
        Safe_Printf("[Cloud CMD] 🤖 AI AUTO MODE RESTORED!\r\n");
    }
}



