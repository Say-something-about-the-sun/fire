/**
 * @file    esp8266_report.c
 * @brief   网络数据上报与指令解析实现
 * @note    负责采集系统各个传感器的数据，构建 JSON 格式报文，并解析下行控制指令。
 */
#include "esp8266_report.h"
#include "ai_decision.h"   
#include "usart3.h"
#include "usart2.h"
#include "smoke.h"
#include "RTC.h"
#include "jpeg_serial.h"
#include "dht11.h"
#include <stdio.h>
#include <string.h>

extern u8 USART3_RX_BUF[];
extern u16 USART3_RX_STA;

extern void Safe_Printf(char *format, ...);

/**
 * @brief  初始化网络上报模块
 */
void ESP8266_Report_Init(void)
{
    Safe_Printf("[ESP8266 Report] Smart MQTT Gateway Mode Initialized.\r\n");
}

/**
 * @brief  采集全局传感器数据并构建内部数据包
 * @param  packet 数据包指针
 */
void ESP8266_Report_CollectSensorData(SensorDataPacket* packet)
{
    RTC_Get_Time(packet->timestamp);
    
    /* 采集环境物理传感器数据 */
    packet->smoke_ppm = Smoke_Get_Concentration();
    packet->smoke_adc = Smoke_Get_ADC_Value();
    packet->smoke_detected = (packet->smoke_ppm > 50.0f) ? 1 : 0;
    
    /* 采集系统运行状态数据 */
    packet->frame_count = jpeg_serial_get_frame_count();
    packet->dropped_frames = jpeg_serial_get_dropped_frames();

    /* 采集 ESP32 协处理器回传数据 */
    packet->flame_adc = g_esp32_data.flame_adc;
    packet->flame_do = g_esp32_data.flame_do;
    packet->fire_detected = g_esp32_data.fire_detected;

    /* 采集温湿度传感器数据 (附带数据异常过滤机制) */
    static u8 last_good_temp = 25, last_good_humi = 50; 
    u8 temp_val = 0, humi_val = 0;
    if(DHT11_Read_Data(&temp_val, &humi_val) == 0) {
        if(humi_val <= 100 && temp_val <= 80) {
            last_good_temp = temp_val;
            last_good_humi = humi_val;
        }
    }
    packet->temperature = (float)last_good_temp;
    packet->humidity = (float)last_good_humi;

    /* 数据收集完毕，调用 AI 决策中枢进行风险评估与响应联动 */
    AI_Fire_Decision_Center(packet);
        
    /* 提取 AI 决策后的最终系统控制状态，准备网络下发 */
    packet->virtual_current   = g_core_state.virtual_current;
    packet->main_power_status = g_core_state.main_power_status;
    packet->system_mode       = g_core_state.mode;
    packet->pump_status       = g_core_state.pump_status;
}

/*
// 解析网络指令的轻量化处理 (已注销，保留备用)
static void Process_Cloud_Commands(void)
{
    if (USART3_RX_STA & 0x8000) 
    {
        USART3_RX_BUF[USART3_RX_STA & 0x3FFF] = '\0'; 
        
        // 直接将字符串交由 AI 决策中枢处理，网络层不直接操作底层硬件
        AI_Execute_Cloud_Command((const char*)USART3_RX_BUF);
        
        memset(USART3_RX_BUF, 0, sizeof(USART3_RX_BUF));
        USART3_RX_STA = 0; 
    }
}
*/

/**
 * @brief  轮询处理云端下发的控制指令
 */
void ESP8266_Report_PollCommands(void)
{
    if (USART3_RX_STA & 0x8000)
    {
        USART3_RX_BUF[USART3_RX_STA & 0x3FFF] = '\0';
        
        /* 调试探针：打印被串口中断判定为有效帧的所有下行数据 */
        Safe_Printf("\r\n[NET_RX_DEBUG] %s\r\n", USART3_RX_BUF);
        
        /* 传递指令字符串至 AI 决策中枢处理 */
        AI_Execute_Cloud_Command((const char*)USART3_RX_BUF);
        
        memset(USART3_RX_BUF, 0, sizeof(USART3_RX_BUF));
        USART3_RX_STA = 0; 
    }
}

/**
 * @brief  将内部数据包打包为 JSON 格式字符串
 * @param  packet 原始传感器数据包
 * @param  json   格式化后的 JSON 数据包
 * @note   为规避浮点数(FPU)格式化异常，在拼接前手动将浮点数拆分为整数与小数部分。
 */
void ESP8266_Report_PackageJSON(SensorDataPacket* packet, JsonDataPacket* json)
{
    /* 浮点数拆分保护逻辑 */
    int smoke_int = (int)packet->smoke_ppm;
    int smoke_dec = (int)((packet->smoke_ppm - smoke_int) * 100);
    if(smoke_dec < 0) smoke_dec = -smoke_dec;

    int temp_int = (int)packet->temperature;
    int temp_dec = (int)((packet->temperature - temp_int) * 10);
    if(temp_dec < 0) temp_dec = -temp_dec;
    
    int hum_int = (int)packet->humidity;
    int hum_dec = (int)((packet->humidity - hum_int) * 10);
    if(hum_dec < 0) hum_dec = -hum_dec;
    
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

    /* JSON 组包，采用平铺结构优化云端解析效率，协议要求以换行符 \n 结尾 */
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
        "\"humidity\":%d.%01d,"         
        "\"virtual_current\":%d.%02d"   
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
        "\"pump_status\":%d,"            
        "\"system_mode\":%d,"            
        "\"main_power_status\":%d"       
        "}\n",  /* 协议要求结尾标识 */
        packet->timestamp,
        packet->flame_adc,
        packet->flame_do,
        packet->smoke_adc,
        smoke_int, smoke_dec,
        packet->smoke_detected ? "true" : "false",
        temp_int, temp_dec,
        packet->temp_detected,
        hum_int, hum_dec,               
        cur_int, cur_dec,               
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
        packet->pump_status,             
        packet->system_mode,             
        packet->main_power_status        
        );
    
    json->json_len = (len > 0) ? (u16)len : 0;
}

/**
 * @brief  上报系统传感器数据
 * @return u8 执行状态
 */
u8 ESP8266_Report_SendSensorData(void)
{
    static SensorDataPacket sensor_packet;
    static JsonDataPacket json_packet; 
    
    /* 获取并封包系统状态 */
    ESP8266_Report_CollectSensorData(&sensor_packet);
    ESP8266_Report_PackageJSON(&sensor_packet, &json_packet);
    
    if (json_packet.json_len == 0) return 1; 
    
    /* 提交底层驱动层发送数据 */
    USART3_Send_Data((u8*)json_packet.json_str, json_packet.json_len);
    
    return 1; 
}
