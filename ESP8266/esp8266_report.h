#ifndef __ESP8266_REPORT_H
#define __ESP8266_REPORT_H

#include "sys.h"
#include "fire_detection.h"




// 传感器数据包结构体
typedef struct {
    char timestamp[20];
    u16 flame_adc;
    u16 flame_do;
    u16 smoke_adc;
    float smoke_ppm;
    u8 smoke_detected;
    u8 temp_detected;
    
    u8 fire_detected;           // ESP32火焰检测
    u8 image_fire_detected;     // 图像火焰检测
    float image_confidence;
    u32 fire_area;
    u16 fire_center_x;
    u16 fire_center_y;
    u8 flicker_detected;
    
    u8 risk_level;
    float confidence;
    u32 frame_count;
    u32 dropped_frames;
		
		
		float virtual_current;  // 虚拟/真实电流 (A)
		float temperature;      // DHT11 温度
    float humidity;         // DHT11 湿度
    u8 pump_status;         // 水泵状态：1=喷水，0=关闭
    
		
		
} SensorDataPacket;

// JSON数据包结构体
typedef struct {
    char json_str[1024];  // 保证 1024 字节防越界
    u16 json_len;
} JsonDataPacket;

// 对外接口声明
void ESP8266_Report_Init(void);
void ESP8266_Report_SendSensorData(void);
void ESP8266_Report_UpdateFireDetectionResult(FireDetectionResult* result);

extern SensorDataPacket g_sys_data;

#endif
