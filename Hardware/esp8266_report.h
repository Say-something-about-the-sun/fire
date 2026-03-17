#ifndef __ESP8266_REPORT_H
#define __ESP8266_REPORT_H

#include "sys.h"
#include "ESP8266.h"
#include "fire_detection.h"

// 传感器数据包结构
typedef struct {
    char timestamp[32];      // 时间戳
    u16 flame_adc;          // 火焰ADC值
    u8 flame_do;            // 火焰数字输出（0=有火，1=无火）
    u16 smoke_adc;          // 烟雾ADC值
    float smoke_ppm;        // 烟雾浓度（PPM）
    u8 smoke_detected;      // 是否检测到烟雾
    u8 temp_detected;       // 是否检测到温度异常
    float temperature;      // 温度（摄氏度）
    u8 fire_detected;       // 是否检测到火焰（ESP32）
    u8 risk_level;          // 风险等级（0=无，1=低，2=中，3=高）
    u8 confidence;          // 置信度（0-100）
    u32 frame_count;        // 处理帧数
    u32 dropped_frames;     // 丢帧数
    
    // 火焰检测详细信息（图像检测）
    u8 image_fire_detected; // 图像检测是否发现火焰
    u8 image_confidence;   // 图像检测置信度
    u16 fire_area;         // 火焰区域面积
    u16 fire_center_x;     // 火焰质心X坐标
    u16 fire_center_y;     // 火焰质心Y坐标
    u8 flicker_detected;   // 是否检测到闪烁
} SensorDataPacket;

// JSON数据包结构
typedef struct {
    char json_str[1024];    // JSON字符串（增大缓冲区）
    u16 json_len;          // JSON长度
} JsonDataPacket;

// 函数声明
void ESP8266_Report_Init(void);
void ESP8266_Report_ReadSmokeSensor(SensorDataPacket* packet);
void ESP8266_Report_RequestESP32Data(SensorDataPacket* packet);
void ESP8266_Report_CalculateRiskLevel(SensorDataPacket* packet);
void ESP8266_Report_UpdateFireDetectionResult(FireDetectionResult* fire_result);
void ESP8266_Report_CollectSensorData(SensorDataPacket* packet);
void ESP8266_Report_PackageJSON(SensorDataPacket* packet, JsonDataPacket* json);
ESP8266_Status ESP8266_Report_SendJSON(JsonDataPacket* json);
void ESP8266_Report_SendSensorData(void);

#endif
