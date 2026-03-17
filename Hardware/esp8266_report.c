#include "esp8266_report.h"
#include "usart3.h"
#include "usart2.h"
#include "delay.h"
#include "RTC.h"
#include "smoke.h"
#include "jpeg_serial.h"
#include "ESP8266.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h" 


// 外部变量声明
extern u8 ESP32_RX_BUF[];
extern u16 ESP32_RX_STA;
extern u8 USART3_RX_BUF[];
extern u16 USART3_RX_STA;

// 外部函数声明
extern void RTC_Get_Time(char* str);
extern void RTC_Set_Time(char* time_str);

// 全局变量（火焰检测结果的最新值）
static FireDetectionResult g_latest_fire_result = {0};

// WiFi和服务器配置
#define WIFI_SSID       "TP-LINK_FF99"
#define WIFI_PASSWORD   "pq18782975992"
#define SERVER_IP       "192.168.0.102"
#define SERVER_PORT     8080

// 初始化ESP8266报告模块
void ESP8266_Report_Init(void)
{
    ESP8266_Status status;
    u8 retry_count = 0;
    
    memset(&g_latest_fire_result, 0, sizeof(FireDetectionResult));
    printf("[ESP8266 Report] Module initializing...\r\n");
    
    // 0. 启用DHCP
    printf("[ESP8266 Report] Enabling DHCP...\r\n");
    status = ESP8266_EnableDHCP();
    if(status != ESP8266_OK)
    {
        printf("[ESP8266 Report] Failed to enable DHCP\r\n");
    }
    delay_ms(1000);
    
    // 1. 连接WiFi
    printf("[ESP8266 Report] Connecting to WiFi: %s\r\n", WIFI_SSID);
    while(retry_count < 3)
    {
        status = ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
        if(status == ESP8266_OK)
        {
            printf("[ESP8266 Report] WiFi connected\r\n");
            break;
        }
        retry_count++;
        printf("[ESP8266 Report] WiFi connection failed, retry %d/3\r\n", retry_count);
        delay_ms(2000);
    }
    
    if(retry_count >= 3)
    {
        printf("[ESP8266 Report] WiFi connection failed after 3 retries\r\n");
        return;
    }
    
    // 2. 获取IP地址
    ESP8266_GetIP();
    delay_ms(1000);
    
    // 3. 设置为单连接模式（必须在连接服务器之前设置）
    printf("[ESP8266 Report] Setting single connection mode...\r\n");
    status = ESP8266_SetMuxMode(0);
    if(status != ESP8266_OK)
    {
        printf("[ESP8266 Report] Failed to set single connection mode\r\n");
        return;
    }
    delay_ms(500);
    
    // 4. 连接服务器
    printf("[ESP8266 Report] Connecting to server: %s:%d\r\n", SERVER_IP, SERVER_PORT);
    retry_count = 0;
    while(retry_count < 3)
    {
        status = ESP8266_ConnectServer("TCP", SERVER_IP, SERVER_PORT);
        if(status == ESP8266_OK)
        {
            printf("[ESP8266 Report] Server connected\r\n");
            break;
        }
        retry_count++;
        printf("[ESP8266 Report] Server connection failed, retry %d/3\r\n", retry_count);
        delay_ms(2000);
    }
    
    if(retry_count >= 3)
    {
        printf("[ESP8266 Report] Server connection failed after 3 retries\r\n");
        return;
    }
    
    // 5. 保持常规AT命令模式（不使用透传）
    printf("[ESP8266 Report] Using regular AT command mode\r\n");
    
    // 验证连接状态
    status = ESP8266_SendATCommand("AT+CIPSTATUS\r\n", "STATUS:CONNECTED", 1000);
    if(status != ESP8266_OK)
    {
        printf("[ESP8266 Report] Connection status check failed\r\n");
        // 尝试重新连接
        status = ESP8266_ConnectServer("TCP", SERVER_IP, SERVER_PORT);
        if(status != ESP8266_OK)
        {
            printf("[ESP8266 Report] Reconnection failed\r\n");
            return;
        }
    }
    
    printf("[ESP8266 Report] Connection status verified\r\n");
    printf("[ESP8266 Report] Module initialized successfully\r\n");
}

// 读取烟雾传感器数据
void ESP8266_Report_ReadSmokeSensor(SensorDataPacket* packet)
{
    u16 smoke_adc = Smoke_Get_ADC_Value();
    float smoke_ppm = Smoke_Get_Concentration();
    
    packet->smoke_adc = smoke_adc;
    packet->smoke_ppm = smoke_ppm;
    
    // 烟雾检测逻辑
    if(smoke_ppm > 50.0)
    {
        packet->smoke_detected = 1;
    }
    else
    {
        packet->smoke_detected = 0;
    }
}

// 从ESP32获取火焰传感器数据
void ESP8266_Report_RequestESP32Data(SensorDataPacket* packet)
{
    // 检查ESP32是否发送数据（使用中断接收的方式）
    if((ESP32_RX_STA & 0x8000) != 0)
    {
        // 结束字符串
        ESP32_RX_BUF[ESP32_RX_STA & 0x7FFF] = '\0';
        
        // 检查是否是时间数据
        if(strncmp((char*)ESP32_RX_BUF, "TIME:", 5) == 0)
        {
            // 处理时间同步数据
            char* time_str = (char*)ESP32_RX_BUF + 5;
            if(time_str)
            {
                RTC_Set_Time(time_str);
            }
        }
        else
        {
            // 解析火焰传感器数据
            char* flame_ao_str = strstr((char*)ESP32_RX_BUF, "FLAME_AO:");
            char* flame_do_str = strstr((char*)ESP32_RX_BUF, "FLAME_DO:");
            char* fire_str = strstr((char*)ESP32_RX_BUF, "FIRE:");
            
            if(flame_ao_str && flame_do_str && fire_str)
            {
                // 提取FLAME_AO值（跳过FLAME_AO:）
                u16 flame_ao = atoi(flame_ao_str + 9);
                
                // 提取FLAME_DO值（跳过FLAME_DO:）
                u8 flame_do = atoi(flame_do_str + 9);
                
                // 提取FIRE值（跳过FIRE:）
                u8 fire = atoi(fire_str + 5);
                
                // 填充数据包
                packet->flame_adc = flame_ao;
                packet->flame_do = flame_do;
                packet->fire_detected = fire;
                
                // 发送确认信号（使用USART2）
                USART2_Send_String("OK\r\n");
            }
        }
        
        // 清空接收缓冲区
        ESP32_RX_STA = 0;
    }
}

// 计算风险等级
void ESP8266_Report_CalculateRiskLevel(SensorDataPacket* packet)
{
    u8 risk_level = 0;
    u8 confidence = 0;
    
    // 基于检测结果计算风险等级
    if(packet->fire_detected || packet->image_fire_detected)
    {
        risk_level = 3;  // 高风险
        confidence = 90;
    }
    else if(packet->smoke_detected)
    {
        risk_level = 2;  // 中风险
        confidence = 75;
    }
    else if(packet->temp_detected)
    {
        risk_level = 1;  // 低风险
        confidence = 60;
    }
    
    packet->risk_level = risk_level;
    packet->confidence = confidence;
}

// 更新火焰检测结果（从图像检测）
void ESP8266_Report_UpdateFireDetectionResult(FireDetectionResult* fire_result)
{
    if(fire_result != NULL)
    {
        memcpy(&g_latest_fire_result, fire_result, sizeof(FireDetectionResult));
    }
}

// 采集传感器数据
void ESP8266_Report_CollectSensorData(SensorDataPacket* packet)
{
    // 1. 读取烟雾传感器数据
    ESP8266_Report_ReadSmokeSensor(packet);
    
    // 2. 请求ESP32火焰传感器数据（暂时禁用，隔离问题）
    // ESP8266_Report_RequestESP32Data(packet);
    // delay_ms(100);  // 等待ESP32响应
    
    // 临时填充ESP32数据（模拟数据）
    packet->flame_adc = 34500;
    packet->flame_do = 0;
    packet->fire_detected = 0;
    packet->temperature = 25.5;
    
    // 3. 填充时间戳
    RTC_Get_Time(packet->timestamp);
    
    // 4. 填充火焰检测详细信息（图像检测）
    packet->image_fire_detected = g_latest_fire_result.fire_detected;
    packet->image_confidence = g_latest_fire_result.confidence;
    packet->fire_area = g_latest_fire_result.fire_area;
    packet->fire_center_x = g_latest_fire_result.fire_center_x;
    packet->fire_center_y = g_latest_fire_result.fire_center_y;
    packet->flicker_detected = g_latest_fire_result.flicker_detected;
    
    // 5. 填充帧计数
    packet->frame_count = jpeg_serial_get_frame_count();
    packet->dropped_frames = jpeg_serial_get_dropped_frames();
    
    // 6. 计算风险等级
    ESP8266_Report_CalculateRiskLevel(packet);
}


// JSON拼装 - 禁绝浮点数、防溢出版
void ESP8266_Report_PackageJSON(SensorDataPacket* packet, JsonDataPacket* json)
{
    // 【关键修复 2】：手动拆解浮点数，避开 %f 的对齐死机陷阱！
    int smoke_int = (int)packet->smoke_ppm;
    int smoke_dec = (int)((packet->smoke_ppm - smoke_int) * 100);
    if (smoke_dec < 0) smoke_dec = -smoke_dec;

    int temp_int = (int)packet->temperature;
    int temp_dec = (int)((packet->temperature - temp_int) * 10);
    if (temp_dec < 0) temp_dec = -temp_dec;

    // 清空缓冲区
    memset(json->json_str, 0, sizeof(json->json_str));

    // 【关键修复 3】：使用安全的 snprintf，并且用 %d.%02d 代替 %.2f
    int len = snprintf(json->json_str, sizeof(json->json_str),
        "{"
        "\"device\":\"fire_monitor_001\","
        "\"timestamp\":\"%s\","
        "\"sensors\":{"
        "\"flame_adc\":%d,"
        "\"flame_do\":%d,"
        "\"smoke_adc\":%d,"
        "\"smoke_ppm\":%d.%02d,"     // <--- 纯整数拼接小数
        "\"smoke_detected\":%s,"
        "\"temperature\":%d.%01d,"   // <--- 纯整数拼接小数
        "\"temp_detected\":%d"
        "},"
        "\"fire_detection\":{"
        "\"esp32_fire_detected\":%s,"
        "\"image_fire_detected\":%s,"
        "\"image_confidence\":%d,"
        "\"fire_area\":%d,"
        "\"fire_center_x\":%d,"
        "\"fire_center_y\":%d,"
        "\"flicker_detected\":%d"
        "},"
        "\"risk_level\":%d,"
        "\"confidence\":%d,"
        "\"frame_count\":%lu,"
        "\"dropped_frames\":%lu"
        "}\r\n",
        packet->timestamp,
        packet->flame_adc,
        packet->flame_do,
        packet->smoke_adc,
        smoke_int, smoke_dec,        // <--- 传入整数
        packet->smoke_detected ? "true" : "false",
        temp_int, temp_dec,          // <--- 传入整数
        packet->temp_detected,
        packet->fire_detected ? "true" : "false",
        packet->image_fire_detected ? "true" : "false",
        packet->image_confidence,
        packet->fire_area,
        packet->fire_center_x,
        packet->fire_center_y,
        packet->flicker_detected,
        packet->risk_level,
        packet->confidence,
        (unsigned long)packet->frame_count,
        (unsigned long)packet->dropped_frames
    );
    
    if(len > 0) json->json_len = (u16)len;
    else json->json_len = 0;
}


// 发送JSON数据（终极抗丢包、去状态查询版）
ESP8266_Status ESP8266_Report_SendJSON(JsonDataPacket* json)
{
    char cmd[32];
    
    // 1. 🚨 彻底删掉致命的 AT+CIPSTATUS 检查！
    // 不去问 ESP8266 连着没有，因为高负载下极易漏读导致死锁。直接开始发！
    
    // 2. 告诉 ESP8266 准备接收多长的数据
    sprintf(cmd, "AT+CIPSEND=%d\r\n", json->json_len);
    
    // 清空接收区
    USART3_RX_STA = 0;
    memset(USART3_RX_BUF, 0, USART3_RX_BUF_SIZE);
    
    // 发送指令
    USART3_Send_String(cmd);
    
    // 3. 【核心盲发】：强制延时 50 毫秒，等待 ESP8266 弹出 '>' 符号。
    // 绝对不用 while 循环去串口里检索 '>'，因为 MCU 忙着处理图片时容易把 '>' 漏掉。
    // 50 毫秒在硬件上足够 ESP8266 准备好通道了！
    delay_ms(50);
    
    // 4. 闭着眼睛直接把 JSON 数据灌入串口！
    // ESP8266 会在底层照单全收，并自动转发给你的 TCP 助手。
    USART3_Send_Data((u8*)json->json_str, json->json_len);
    
    // 5. 让数据飞一会儿
    delay_ms(100);
    
    // 6. 事后像冲马桶一样，暴力清空串口接收区！
    // 把残留的 "SEND OK" 之类的废话全部丢弃，保证下一次发送环境绝对干净。
    USART3_RX_STA = 0;
    memset(USART3_RX_BUF, 0, USART3_RX_BUF_SIZE);
    
    return ESP8266_OK;
}

// 统一发送传感器数据接口
void ESP8266_Report_SendSensorData(void)
{
    static SensorDataPacket sensor_packet;
    static JsonDataPacket json_packet;
    
    //printf("[ESP8266 Report] Starting to send sensor data...\r\n");
    
    // 1. 采集传感器数据
    printf("[ESP8266 Report] Collecting sensor data...\r\n");
    ESP8266_Report_CollectSensorData(&sensor_packet);
    
    // 2. JSON拼装
    printf("[ESP8266 Report] Packaging JSON...\r\n");
    ESP8266_Report_PackageJSON(&sensor_packet, &json_packet);
    
    // 打印JSON内容（调试用）
    //printf("[ESP8266 Report] JSON content: %s\r\n", json_packet.json_str);
    
    // 3. 发送JSON数据
    printf("[ESP8266 Report] Sending JSON (%d bytes)...\r\n", json_packet.json_len);
    ESP8266_Report_SendJSON(&json_packet);
    
    printf("[ESP8266 Report] Sensor data sent completed\r\n");
}
