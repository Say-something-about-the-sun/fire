#include "esp8266_report.h"
#include "usart3.h"
#include "smoke.h"
#include "RTC.h"
#include "jpeg_serial.h"
#include <stdio.h>
#include <string.h>


FireDetectionResult g_latest_fire_result;


// 1. 初始化模块 (现在只需打印提示即可，真实的硬件初始化在 main 里的 USART3_Init)
void ESP8266_Report_Init(void)
{
    printf("[ESP8266 Report] Smart MQTT Gateway Mode Initialized.\r\n");
}

// 2. 采集传感器数据 
static void ESP8266_Report_CollectSensorData(SensorDataPacket* packet)
{
    // 获取时间
    RTC_Get_Time(packet->timestamp);
    
    // 获取烟雾
    packet->smoke_ppm = Smoke_Get_Concentration();
    packet->smoke_adc = Smoke_Get_ADC_Value();
    packet->smoke_detected = (packet->smoke_ppm > 50.0f) ? 1 : 0;
    
    // 临时填充其他数据（请根据你的实际情况接入真实函数）
    packet->flame_adc = 34500;
    packet->flame_do = 0;
    packet->fire_detected = 0;
    packet->temperature = 25.5f;
    packet->temp_detected = 1;
    
    // 获取图像处理结果
    packet->image_fire_detected = g_latest_fire_result.fire_detected;
    packet->image_confidence = g_latest_fire_result.confidence;
    packet->fire_area = g_latest_fire_result.fire_area;
    packet->fire_center_x = g_latest_fire_result.fire_center_x;
    packet->fire_center_y = g_latest_fire_result.fire_center_y;
    packet->flicker_detected = g_latest_fire_result.flicker_detected;
    
    // 帧统计
    packet->frame_count = jpeg_serial_get_frame_count();
    packet->dropped_frames = jpeg_serial_get_dropped_frames();
    
    // 风险等级 (临时定死，或者接入你的计算函数)
    packet->risk_level = packet->smoke_detected ? 2 : 0;
    packet->confidence = 85.5f;
}

// 3. 将数据打包成 JSON 字符串 (防死机纯整数版)
static void ESP8266_Report_PackageJSON(SensorDataPacket* packet, JsonDataPacket* json)
{
    // 【防 FPU 死机】：手动拆分浮点数
    int smoke_int = (int)packet->smoke_ppm;
    int smoke_dec = (int)((packet->smoke_ppm - smoke_int) * 100);
    if(smoke_dec < 0) smoke_dec = -smoke_dec;

    int temp_int = (int)packet->temperature;
    int temp_dec = (int)((packet->temperature - temp_int) * 10);
    if(temp_dec < 0) temp_dec = -temp_dec;
    
    int img_conf_int = (int)packet->image_confidence;
    int img_conf_dec = (int)((packet->image_confidence - img_conf_int) * 100);
    if(img_conf_dec < 0) img_conf_dec = -img_conf_dec;
    
    int total_conf_int = (int)packet->confidence;
    int total_conf_dec = (int)((packet->confidence - total_conf_int) * 100);
    if(total_conf_dec < 0) total_conf_dec = -total_conf_dec;

    memset(json->json_str, 0, sizeof(json->json_str));

    // 使用 snprintf 安全拼装，注意设备名必须和 INO 里一致！
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
        "\"temp_detected\":%d"
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
        "\"dropped_frames\":%lu"
        "}\n",  // <--- 🚨 核心关键：必须以 \n 结尾，触发 Arduino 解析！
        packet->timestamp,
        packet->flame_adc,
        packet->flame_do,
        packet->smoke_adc,
        smoke_int, smoke_dec,
        packet->smoke_detected ? "true" : "false",
        temp_int, temp_dec,
        packet->temp_detected,
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
        packet->dropped_frames
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
        printf("[ESP8266 Report] Pushed JSON to Gateway (%d bytes)\r\n", json_packet.json_len);
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


