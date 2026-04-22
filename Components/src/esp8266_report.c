#include "esp8266_report.h"
#include "ai_decision.h"   // 引入 AI 大脑
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

void ESP8266_Report_Init(void)
{
    Safe_Printf("[ESP8266 Report] Smart MQTT Gateway Mode Initialized.\r\n");
}

// 纯粹的数据搬运工
void ESP8266_Report_CollectSensorData(SensorDataPacket* packet)
{
    RTC_Get_Time(packet->timestamp);
    
    // 传感器
    packet->smoke_ppm = Smoke_Get_Concentration();
    packet->smoke_adc = Smoke_Get_ADC_Value();
    packet->smoke_detected = (packet->smoke_ppm > 50.0f) ? 1 : 0;
    
   
    packet->frame_count = jpeg_serial_get_frame_count();
    packet->dropped_frames = jpeg_serial_get_dropped_frames();

    // ESP32 数据
    packet->flame_adc = g_esp32_data.flame_adc;
    packet->flame_do = g_esp32_data.flame_do;
    packet->fire_detected = g_esp32_data.fire_detected;

    // 温湿度
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

    // 数据收集完毕，呼叫 AI 大脑进行决策和填装最终状态
    AI_Fire_Decision_Center(packet);
}

/*
// 解析网络指令的轻量化处理
static void Process_Cloud_Commands(void)
{
    if (USART3_RX_STA & 0x8000) 
    {
        USART3_RX_BUF[USART3_RX_STA & 0x3FFF] = '\0'; 
        
        // 直接将字符串扔给 AI 大脑处理，网络层不直接操作硬件！
        AI_Execute_Cloud_Command((const char*)USART3_RX_BUF);
        
        memset(USART3_RX_BUF, 0, sizeof(USART3_RX_BUF));
        USART3_RX_STA = 0; 
    }
}
*/


// 将这个函数名添加到 esp8266_report.h 中：void ESP8266_Report_PollCommands(void);
void ESP8266_Report_PollCommands(void)
{
    if (USART3_RX_STA & 0x8000)
    {
        USART3_RX_BUF[USART3_RX_STA & 0x3FFF] = '\0';
        
        // 🚨 终极调试探针：无论收到什么，只要被中断认定为一帧，就全部打印出来！
        Safe_Printf("\r\n[NET_RX_DEBUG] %s\r\n", USART3_RX_BUF);
        
        // 扔给 AI 大脑处理
        AI_Execute_Cloud_Command((const char*)USART3_RX_BUF);
        
        memset(USART3_RX_BUF, 0, sizeof(USART3_RX_BUF));
        USART3_RX_STA = 0; 
    }
}

// 3. 将数据打包成 JSON 字符串 (防死机纯整数版)
// [esp8266_report.c 中替换]
void ESP8266_Report_PackageJSON(SensorDataPacket* packet, JsonDataPacket* json)
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






u8 ESP8266_Report_SendSensorData(void)
{
    static SensorDataPacket sensor_packet;
    static JsonDataPacket json_packet; 
    
    // 1. 处理积压的云端指令
    //Process_Cloud_Commands();

    // 2. 采集并打包
    ESP8266_Report_CollectSensorData(&sensor_packet);
    ESP8266_Report_PackageJSON(&sensor_packet, &json_packet); // 调用你的原函数
    
    if (json_packet.json_len == 0) return 1; 
    
    USART3_Send_Data((u8*)json_packet.json_str, json_packet.json_len);
    //Safe_Printf("[ESP8266 Report] Pushed JSON to Gateway (%d bytes)\r\n", json_packet.json_len);
    
    return 1; 
}

