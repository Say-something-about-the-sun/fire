/**
 * @file    esp8266_report.h
 * @brief   网络数据上报与指令解析协议头文件
 */
#ifndef __ESP8266_REPORT_H
#define __ESP8266_REPORT_H

#include "sys.h"
#include "fire_detection.h"

/* 传感器数据包结构体 (用于内部状态聚合) */
typedef struct {
    char  timestamp[20];
    u16   flame_adc;
    u16   flame_do;
    u16   smoke_adc;
    float smoke_ppm;
    u8    smoke_detected;
    u8    temp_detected;
    
    u8    fire_detected;           /* ESP32物理传感器火焰检测标志 */
    u8    image_fire_detected;     /* 视觉算法火焰检测标志 */
    float image_confidence;        /* 视觉检测置信度 */
    u32   fire_area;               /* 火焰特征面积 */
    u16   fire_center_x;           /* 火焰特征质心X坐标 */
    u16   fire_center_y;           /* 火焰特征质心Y坐标 */
    u8    flicker_detected;        /* 闪烁特征检测标志 */
    
    u8    risk_level;              /* 综合评估危险等级 */
    float confidence;              /* 综合评估置信度 */
    u32   frame_count;             /* 成功处理的图像帧数 */
    u32   dropped_frames;          /* 丢弃的图像帧数 */
        
    float virtual_current;         /* 虚拟/真实电流采样值 (A) */
    float temperature;             /* DHT11 温度数据 */
    float humidity;                /* DHT11 湿度数据 */
    u8    pump_status;             /* 执行器(水泵)状态：1=运行，0=关闭 */
    
    u8    system_mode;             /* 运行模式: 0=自动(AI托管), 1=手动(人工接管) */
    u8    main_power_status;       /* 电源状态: 1=主电源正常, 0=过载跳闸 */
} SensorDataPacket;

/* JSON数据包结构体 (用于外部网络传输) */
typedef struct {
    char json_str[1024];           /* 预留1024字节缓冲区以防止字符串越界 */
    u16  json_len;
} JsonDataPacket;

/* 外部接口声明 */
void ESP8266_Report_Init(void);
u8   ESP8266_Report_SendSensorData(void);
void ESP8266_Report_UpdateFireDetectionResult(FireDetectionResult* result);
void button_scan_task(void *pvParameters);

void ESP8266_Report_PackageJSON(SensorDataPacket* packet, JsonDataPacket* json);
void ESP8266_Report_CollectSensorData(SensorDataPacket* packet);
void ESP8266_Report_PollCommands(void);

extern SensorDataPacket g_sys_data;

#endif /* __ESP8266_REPORT_H */
