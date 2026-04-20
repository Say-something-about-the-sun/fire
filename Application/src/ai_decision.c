// ai_decision.c
#include "ai_decision.h"
#include "water_pump.h"
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// 内部私有全局变量（摒弃 extern）
static SystemCoreState_t g_core_state;
extern FireDetectionResult g_latest_fire_result; // 暂时保留，由 JPEG 任务更新
static FireDetectionResult s_latest_vision_result; // 保存最新的图像检测结果





extern void Safe_Printf(char *format, ...);

// 核心状态保护锁
static SemaphoreHandle_t Mutex_CoreState = NULL;




static float base_temp = -100.0f; 
static float base_smoke = -1.0f;
static u16 calib_samples = 0;
#define CALIB_MAX_SAMPLES 6  // 假设 5 秒采集一次，6次即前 30 秒为校准期






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
    float total_score = 0.0f;

    // ==========================================
    // 0. 关键修复：将异步的视觉缓存数据注入到当前的 packet 中！
    // 否则网络层拿到的 JSON 永远没有图像识别数据
    // ==========================================
    packet->image_fire_detected = s_latest_vision_result.fire_detected;
    packet->image_confidence    = s_latest_vision_result.confidence;
    packet->fire_area           = s_latest_vision_result.fire_area;
    packet->fire_center_x       = s_latest_vision_result.fire_center_x;
    packet->fire_center_y       = s_latest_vision_result.fire_center_y;
    packet->flicker_detected    = s_latest_vision_result.flicker_detected;

    // ==========================================
    // 1. 系统开机基线校准阶段 (前30秒)
    // ==========================================
    if (calib_samples < CALIB_MAX_SAMPLES) {
        if (calib_samples == 0) {
            base_temp = packet->temperature; // 修复：temp -> temperature
            base_smoke = packet->smoke_ppm;
        } else {
            // 滑动平均平滑基线
            base_temp = (base_temp * 0.7f) + (packet->temperature * 0.3f);
            base_smoke = (base_smoke * 0.7f) + (packet->smoke_ppm * 0.3f);
        }
        calib_samples++;
        
        // 绝对红线：即使在校准期，数值爆表也必须立刻中断校准并报警
        if (packet->temperature > 60.0f || packet->smoke_ppm > 500.0f || packet->image_fire_detected) {
            calib_samples = CALIB_MAX_SAMPLES; 
        } else {
            packet->risk_level = 0; 
            return; 
        }
    }

    // ==========================================
    // 2. 环境缓慢漂移修正
    // ==========================================
    if (packet->temperature < 40.0f && packet->smoke_ppm < 100.0f) {
        // 适应白天/黑夜的缓慢温差
        base_temp = (base_temp * 0.99f) + (packet->temperature * 0.01f);
        base_smoke = (base_smoke * 0.99f) + (packet->smoke_ppm * 0.01f);
    }

    // ==========================================
    // 3. 多维加权打分系统 (满分 100+)
    // ==========================================
    // 维度A：视觉 AI (权重极高)
    if (packet->image_fire_detected) {
        total_score += 70.0f; 
        // 闪烁特征附加分
        if (packet->flicker_detected) total_score += 15.0f;
    }

    // 维度B：温度 (突变与绝对值)
    float delta_temp = packet->temperature - base_temp;
    if (packet->temperature >= 65.0f) total_score += 50.0f;      // 绝对红线
    else if (delta_temp > 15.0f) total_score += 40.0f;           // 突变
    else if (delta_temp > 5.0f) total_score += 15.0f;            // 疑似

    // 维度C：烟雾 (突变与绝对值)
    float delta_smoke = packet->smoke_ppm - base_smoke;
    if (packet->smoke_ppm >= 300.0f) total_score += 40.0f;       // 绝对红线
    else if (delta_smoke > 100.0f) total_score += 30.0f;         // 突变
    else if (delta_smoke > 30.0f) total_score += 10.0f;          // 疑似

    // ==========================================
    // 4. 决策输出与联动控制
    // ==========================================
    g_core_state.total_confidence = total_score;
    packet->confidence = total_score; // 将总置信度输出到网络层

    if (total_score >= 80.0f) {
        packet->risk_level = 3; // 极度危险
        packet->fire_detected = 1;
        
        if (g_core_state.mode == SYS_MODE_AUTO && g_core_state.pump_status == 0) {
            g_core_state.pump_status = 1;
            WaterPump_On();
            Safe_Printf("[AI] Critical Risk! Auto-triggering pump.\r\n");
        }
    } 
    else if (total_score >= 40.0f) {
        packet->risk_level = 2; // 警告
        packet->fire_detected = 0;
    } 
    else {
        packet->risk_level = 0; // 安全
        packet->fire_detected = 0;
        
        if (g_core_state.mode == SYS_MODE_AUTO && g_core_state.pump_status == 1) {
            g_core_state.pump_status = 0;
            WaterPump_Off();
            Safe_Printf("[AI] Environment safe. Auto-stopping pump.\r\n");
        }
    }
}



// ==================================================================
// 🌩️ 云端指令执行器（精准匹配物理探针抓取的真实数据）
// ==================================================================
void AI_Execute_Cloud_Command(const char* json_str)
{
    // 1. 拦截【强制开启水泵】 -> 对应你抓到的 CMD:PUMP:1
    if (strstr(json_str, "CMD:PUMP:1") != NULL) 
    {	
				if (g_core_state.pump_status == 1 && g_core_state.mode == SYS_MODE_MANUAL) return;
			
        g_core_state.mode = SYS_MODE_MANUAL; 
        g_core_state.pump_status = 1;
        WaterPump_On();
        Safe_Printf("[Cloud CMD] 🚨 MANUAL OVERRIDE: PUMP ON!\r\n");
    }
    // 2. 拦截【强制关闭水泵】 -> 对应你抓到的 CMD:PUMP:0
    else if (strstr(json_str, "CMD:PUMP:0") != NULL) 
    {
				if (g_core_state.pump_status == 0 && g_core_state.mode == SYS_MODE_MANUAL) return;
			
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

// 获取系统核心状态的线程安全副本
SystemCoreState_t AI_Get_Core_State_Safe(void)
{
    SystemCoreState_t copy;
    taskENTER_CRITICAL(); // 进入临界区，打断所有调度
    copy = g_core_state;  // 瞬间完成内存拷贝
    taskEXIT_CRITICAL();  // 退出临界区
    return copy;
}

// 获取视觉结果的线程安全副本（修复 s_latest_vision_result 的撕裂风险）
FireDetectionResult AI_Get_Vision_Result_Safe(void)
{
    FireDetectionResult copy;
    taskENTER_CRITICAL();
    copy = s_latest_vision_result;
    taskEXIT_CRITICAL();
    return copy;
}


