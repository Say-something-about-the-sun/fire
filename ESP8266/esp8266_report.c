#include "esp8266_report.h"
#include "usart3.h"
#include "smoke.h"
#include "RTC.h"
#include "jpeg_serial.h"
#include <stdio.h>
#include "usart2.h"
#include <string.h>
#include "dht11.h"
#include "water_pump.h"
#include "FreeRTOS.h"
#include "key.h"
#include "task.h"


FireDetectionResult g_latest_fire_result;


// ==========================================
// 🌐 定义跨任务共享的全局状态变量
// ==========================================
volatile u8 g_system_mode = 0;       // 0:自动模式, 1:手动模式
volatile u8 g_main_power_status = 1; // 1:通电正常, 0:跳闸断电
volatile float g_virtual_current = 1.2f; // 全局虚拟电流

// ==========================================
// 🚀 极速按键扫描任务 (每 20ms 执行一次，绝对丝滑)
// ==========================================
void button_scan_task(void *pvParameters)
{
    while(1)
    {
        // 1. 获取按键单次触发信号 (带消抖)
        u8 key_val = KEY_Scan(0);

        // 👉 动作 A: [WK_UP 按键] 切换 自动/手动 模式
        if (key_val == WKUP_PRES) {
            g_system_mode = !g_system_mode; 
        }

        // 👉 动作 B: [KEY1 按键] 手动开关水泵
        if (g_system_mode == 1 && key_val == KEY1_PRES) {
            if (WaterPump_GetStatus() == 1) {
                WaterPump_Off(); 
            } else {
                WaterPump_On();  
            }
        }

        // 👉 动作 C: [KEY0 按键] 实时电平监测，模拟电流短路
        // 只要按住不松，电流就一直保持 58.5A
        if (KEY0_VAL == 0) { 
            g_virtual_current = 58.5f;  
        } else {
            g_virtual_current = 1.2f;   
        }

        // 🚨 极其关键：任务休眠 20 毫秒，让出 CPU 给摄像头和网络！
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}


/**
 * @brief  AI 核心决策中枢 (纯逻辑封装)
 * @param  packet: 已经采集好全部数据的结构体指针
 */
static void AI_Fire_Decision_Center(SensorDataPacket* packet)
{
    u8 is_fire_real = packet->image_fire_detected || packet->fire_detected || packet->flame_do == 1;

    // 🔴 Level 3: 确认起火
    if (is_fire_real) 
    {
        packet->risk_level = 3; 
        packet->confidence = packet->image_fire_detected ? packet->image_confidence : 85.0f;
        
        // 只有在自动模式 (0) 下，AI 才允许操作硬件
        if (g_system_mode == 0) {
            if (packet->virtual_current > 10.0) {
                g_main_power_status = 0; // 虚拟跳闸
                WaterPump_Off();         // 禁水
            } else {
                g_main_power_status = 0; // 虚拟跳闸
                WaterPump_On();          // 喷水
            }
        }
    } 
    // 🟠 Level 2: 高危预警 (过载或高温)
    else if (packet->virtual_current > 10.0 || packet->temperature > 60.0)
    {
        packet->risk_level = 2;
        packet->confidence = 90.0f; 
        
        if (g_system_mode == 0) {
            g_main_power_status = 0; // 预防性跳闸
            WaterPump_Off();       
        }
    }
    else 
    {
        packet->risk_level = (packet->smoke_detected) ? 1 : 0;
        packet->confidence = (packet->smoke_detected) ? 60.0f : 100.0f;
        
        if (g_system_mode == 0) { // 【自动恢复正常】
            g_main_power_status = 1; 
            WaterPump_Off();       
        }
    }

    // 将最终的状态打包给发往云端的 packet
    packet->pump_status = WaterPump_GetStatus(); 
    packet->system_mode = g_system_mode;
    packet->main_power_status = g_main_power_status;
}


// 1. 初始化模块 (现在只需打印提示即可，真实的硬件初始化在 main 里的 USART3_Init)
void ESP8266_Report_Init(void)
{
    printf("[ESP8266 Report] Smart MQTT Gateway Mode Initialized.\r\n");
}

// 2. 采集传感器数据 
static void ESP8266_Report_CollectSensorData(SensorDataPacket* packet)
{
    // ================= 1. 获取时间 =================
    // 因为 RTC 已经被 USART2 中断实时校准过了，所以直接读取 RTC 即可获得完美同步的走动时间！
    RTC_Get_Time(packet->timestamp);
    
    // ================= 2. STM32 本地传感器 =================
    // 烟雾传感器 (直连在 STM32)
    packet->smoke_ppm = Smoke_Get_Concentration();
    packet->smoke_adc = Smoke_Get_ADC_Value();
    packet->smoke_detected = (packet->smoke_ppm > 50.0f) ? 1 : 0;
    
    
    // ================= 3. 图像处理结果 (OV5640) =================
    packet->image_fire_detected = g_latest_fire_result.fire_detected;
    packet->image_confidence = g_latest_fire_result.confidence;
    packet->fire_area = g_latest_fire_result.fire_area;
    packet->fire_center_x = g_latest_fire_result.fire_center_x;
    packet->fire_center_y = g_latest_fire_result.fire_center_y;
    packet->flicker_detected = g_latest_fire_result.flicker_detected;
    
    // 帧统计
    packet->frame_count = jpeg_serial_get_frame_count();
    packet->dropped_frames = jpeg_serial_get_dropped_frames();

    // ================= 4. 来自 ESP32 的协处理器数据 =================
    // 直接读取 USART2 中断解析后存入的全局变量
    packet->flame_adc = g_esp32_data.flame_adc;
    packet->flame_do = g_esp32_data.flame_do;
    packet->fire_detected = g_esp32_data.fire_detected;
// ==========================================
		// ==========================================
// ==========================================
		// ==========================================
// 5. 采集温湿度 (带缓存机制)（dht11）
    static u8 last_good_temp = 25; 
    static u8 last_good_humi = 50; 
    u8 temp_val = 0, humi_val = 0;
    
    if(DHT11_Read_Data(&temp_val, &humi_val) == 0) {
        if(humi_val <= 100 && temp_val <= 80) {
            last_good_temp = temp_val;
            last_good_humi = humi_val;
        }
    }
    packet->temperature = (float)last_good_temp;
    packet->humidity = (float)last_good_humi;

    
    // =========================================================
    //  3. 实体按键交互逻辑 (状态机核心)
    // =========================================================
    
   packet->virtual_current = g_virtual_current;
		
		// =========================================================
    //  4. 数据全部就绪，呼叫 AI 大脑进行决策！
    // =========================================================
    AI_Fire_Decision_Center(packet);
}


// 3. 将数据打包成 JSON 字符串 (防死机纯整数版)
// [esp8266_report.c 中替换]
static void ESP8266_Report_PackageJSON(SensorDataPacket* packet, JsonDataPacket* json)
{
    // 【防 FPU 死机】：手动拆分所有浮点数！
    int smoke_int = (int)packet->smoke_ppm;
    int smoke_dec = (int)((packet->smoke_ppm - smoke_int) * 100);
    if(smoke_dec < 0) smoke_dec = -smoke_dec;

    int temp_int = (int)packet->temperature;
    int temp_dec = (int)((packet->temperature - temp_int) * 10);
    if(temp_dec < 0) temp_dec = -temp_dec;
    
    // 新增：湿度拆分
    int hum_int = (int)packet->humidity;
    int hum_dec = (int)((packet->humidity - hum_int) * 10);
    if(hum_dec < 0) hum_dec = -hum_dec;
    
    // 新增：虚拟电流拆分
    int cur_int = (int)packet->virtual_current;
    int cur_dec = (int)((packet->virtual_current - cur_int) * 100);
    if(cur_dec < 0) cur_dec = -cur_dec;

    int img_conf_int = (int)packet->image_confidence;
    int img_conf_dec = (int)((packet->image_confidence - img_conf_int) * 100);
    if(img_conf_dec < 0) img_conf_dec = -img_conf_dec;
    
    int total_conf_int = (int)packet->confidence;
    int total_conf_dec = (int)((packet->confidence - total_conf_int) * 100);
    if(total_conf_dec < 0) total_conf_dec = -total_conf_dec;

    memset(json->json_str, 0, sizeof(json->json_str));

    // 使用 snprintf 安全拼装，扁平化结构，不带 {"value":}，方便 ESP8266 解析
    int len = snprintf(json->json_str, sizeof(json->json_str),
        "{"
        "\"device\":\"stm32f407zgt6\","
        "\"timestamp\":\"%s\","
        "\"sensors\":{"
        "\"flame_adc\":%d,"
        "\"flame_do\":%d,"
        "\"smoke_adc\":%d,"
        "\"smoke_ppm\":%d.%02d,"
        "\"smoke_detected\":%s,"
        "\"temperature\":%d.%01d,"
        "\"temp_detected\":%d,"
        "\"humidity\":%d.%01d,"         // 👈 注入湿度
        "\"virtual_current\":%d.%02d"   // 👈 注入虚拟电流
        "},"
        "\"fire_detection\":{"
        "\"esp32_fire_detected\":%s,"
        "\"image_fire_detected\":%s,"
        "\"image_confidence\":%d.%02d,"
        "\"fire_area\":%lu,"
        "\"fire_center_x\":%d,"
        "\"fire_center_y\":%d,"
        "\"flicker_detected\":%d"
        "},"
        "\"risk_level\":%d,"
        "\"confidence\":%d.%02d,"
        "\"frame_count\":%lu,"
        "\"dropped_frames\":%lu,"
        "\"pump_status\":%d,"            // 👈 注入水泵状态
        "\"system_mode\":%d,"            // 👈 新增：系统模式 (0自动/1手动)
        "\"main_power_status\":%d"       // 👈 新增：主电网跳闸状态
				
				"}\n",  // <--- 🚨 核心关键：必须以 \n 结尾！
        packet->timestamp,
        packet->flame_adc,
        packet->flame_do,
        packet->smoke_adc,
        smoke_int, smoke_dec,
        packet->smoke_detected ? "true" : "false",
        temp_int, temp_dec,
        packet->temp_detected,
        hum_int, hum_dec,               // 湿度参数
        cur_int, cur_dec,               // 电流参数
        packet->fire_detected ? "true" : "false",
        packet->image_fire_detected ? "true" : "false",
        img_conf_int, img_conf_dec,
        packet->fire_area,
        packet->fire_center_x,
        packet->fire_center_y,
        packet->flicker_detected,
        packet->risk_level,
        total_conf_int, total_conf_dec,
        packet->frame_count,
        packet->dropped_frames,
        packet->pump_status,             // 水泵状态参数
				packet->system_mode,             // 👈 填入系统模式变量
        packet->main_power_status        // 👈 填入主电源变量
		);
    
    json->json_len = (len > 0) ? (u16)len : 0;
}

// 4. 统一发送接口 (提供给 FreeRTOS 任务调用)
void ESP8266_Report_SendSensorData(void)
{
    // 【防栈溢出】：必须使用 static
    static SensorDataPacket sensor_packet;
    static JsonDataPacket json_packet; 
    
    // 1. 采集并打包
    ESP8266_Report_CollectSensorData(&sensor_packet);
    ESP8266_Report_PackageJSON(&sensor_packet, &json_packet);
    
    // 2. 直接扔给 USART3 透传给 ESP8266
    if (json_packet.json_len > 0)
    {
        USART3_Send_Data((u8*)json_packet.json_str, json_packet.json_len);
Safe_Printf("[ESP8266 Report] Pushed JSON to Gateway (%d bytes)\r\n", json_packet.json_len);
    }
}

// 接收来自 jpeg_serial.c 的图像处理结果
void ESP8266_Report_UpdateFireDetectionResult(FireDetectionResult* result)
{
    if(result != NULL)
    {
        // 将最新的图像检测结果保存到全局变量中
        g_latest_fire_result.fire_detected = result->fire_detected;
        g_latest_fire_result.confidence = result->confidence;
        g_latest_fire_result.fire_area = result->fire_area;
        g_latest_fire_result.fire_center_x = result->fire_center_x;
        g_latest_fire_result.fire_center_y = result->fire_center_y;
        g_latest_fire_result.flicker_detected = result->flicker_detected;
    }
}


