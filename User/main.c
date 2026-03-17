// main.c - 主程序 - 完整集成版本
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "usart2.h"
#include "usart3.h"
#include "ov5640.h"
#include "dcmi.h"
#include "sdcard_jpeg.h"
#include "extsram.h"
#include "jpeg_serial.h"
#include "smoke.h"
#include "ESP8266.h"
#include "esp8266_report.h"
#include "RTC.h"
#include "sd_data_manager.h"
#include "led.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


// 摄像头参数
#define CAMERA_WIDTH  320
#define CAMERA_HEIGHT 240

// 系统参数
#define SENSOR_INTERVAL    5000    // 传感器采集间隔（ms）- 每5秒发送一次
#define DATA_SAVE_INTERVAL 1000    // 数据保存间隔（ms）
#define DAILY_REPORT_INTERVAL 86400000  // 日报生成间隔（24小时）
#define CLEANUP_INTERVAL   86400000  // 清理数据间隔（24小时）

// 外部变量声明
extern volatile u32 g_raw_frame_count;  // 硬件触发次数
extern volatile u32 g_processed_frame_count;  // 实际处理成功的帧数
extern volatile u8 dma_transfer_complete;   // DMA传输完成标志
extern volatile BufferControl buf_ctrl_a;  // 缓冲区A控制结构
extern volatile BufferControl buf_ctrl_b;  // 缓冲区B控制结构

// 全局变量
SensorRecord g_current_record;      // 当前传感器数据记录

// 统计变量（用于日报生成）
u32 g_record_count = 0;             // 今日记录计数
u32 g_fire_alert_count = 0;         // 火灾告警计数
u32 g_smoke_alert_count = 0;        // 烟雾告警计数
u32 g_temp_alert_count = 0;         // 温度告警计数
u32 g_smoke_adc_sum = 0;            // 烟雾ADC值总和
u16 g_max_smoke_adc = 0;            // 最大烟雾ADC值
u16 g_max_flame_adc = 0;            // 最大火焰ADC值
float g_temp_sum = 0;               // 温度总和
float g_max_temperature = 0;        // 最高温度


// OV5640色调设置函数（参考探索者项目）
void OV5640_Hue_Set(u8 hue)
{
    switch(hue)
    {
        case 0:  // 0度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x80);
            OV5640_WR_Reg(0x5582, 0x00);
            OV5640_WR_Reg(0x5588, 0x01);
            break;
        case 1:  // +30度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x6F);
            OV5640_WR_Reg(0x5582, 0x40);
            OV5640_WR_Reg(0x5588, 0x01);
            break;
        case 2:  // +60度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x40);
            OV5640_WR_Reg(0x5582, 0x6F);
            OV5640_WR_Reg(0x5588, 0x01);
            break;
        case 3:  // +90度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x00);
            OV5640_WR_Reg(0x5582, 0x80);
            OV5640_WR_Reg(0x5588, 0x01);
            break;
        case 4:  // -30度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x6F);
            OV5640_WR_Reg(0x5582, 0x40);
            OV5640_WR_Reg(0x5588, 0x02);
            break;
        case 5:  // -60度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x40);
            OV5640_WR_Reg(0x5582, 0x6F);
            OV5640_WR_Reg(0x5588, 0x02);
            break;
        case 6:  // -90度
            OV5640_WR_Reg(0x5001, 0xFF);
            OV5640_WR_Reg(0x5580, 0x01);
            OV5640_WR_Reg(0x5581, 0x00);
            OV5640_WR_Reg(0x5582, 0x80);
            OV5640_WR_Reg(0x5588, 0x02);
            break;
        default:
            break;
    }
}

// 生成日报
void generate_daily_report(void)
{
    DailyReport report;
    
    // 填充日期
    SD_DataManager_GetCurrentDateString(report.date, sizeof(report.date));
    
    // 填充统计数据
    report.total_records = g_record_count;
    report.fire_alerts = g_fire_alert_count;
    report.smoke_alerts = g_smoke_alert_count;
    report.temp_alerts = g_temp_alert_count;
    
    if(g_record_count > 0)
    {
        report.avg_smoke_ppm = (float)g_smoke_adc_sum / g_record_count;
        report.avg_temperature = g_temp_sum / g_record_count;
    }
    else
    {
        report.avg_smoke_ppm = 0;
        report.avg_temperature = 0;
    }
    
    report.max_smoke_adc = g_max_smoke_adc;
    report.max_flame_adc = g_max_flame_adc;
    report.max_temperature = g_max_temperature;
    
    // 计算最高风险等级
    if(g_fire_alert_count > 0)
        report.max_risk_level = 3;
    else if(g_smoke_alert_count > 0)
        report.max_risk_level = 2;
    else if(g_temp_alert_count > 0)
        report.max_risk_level = 1;
    else
        report.max_risk_level = 0;
    
    // 保存日报
    SD_DataManager_SaveDailyReport(&report);
    
    // 重置统计
    g_record_count = 0;
    g_fire_alert_count = 0;
    g_smoke_alert_count = 0;
    g_temp_alert_count = 0;
    g_smoke_adc_sum = 0;
    g_temp_sum = 0;
    g_max_smoke_adc = 0;
    g_max_flame_adc = 0;
    g_max_temperature = 0;
    
    printf("[Daily Report] Generated and saved!\r\n");
}


int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    delay_init(168);
    uart_init(921600);
    
    // 初始化LED
    LED_Init();
    
    // printf("\r\n========================================\r\n");
    // printf("火灾监测系统 - 完整集成版本\r\n");
    // printf("========================================\r\n\r\n");
    
    // 1. 初始化RTC
    // printf("[1] Initializing RTC...\r\n");
    RTC_Init_Custom();
    
    // 2. 初始化JPEG和串口系统
    // printf("[2] Initializing JPEG Serial System...\r\n");
    jpeg_serial_init();
    
    // 3. 初始化摄像头
    // printf("[3] Initializing OV5640...\r\n");
    if( OV5640_Init() != 0) {
        printf("[ERROR] OV5640 initialization failed!\r\n");
        while(1);
    }
    
    // OV5640_Init()已经设置了JPEG模式和基本参数，这里只需要设置输出尺寸
    // printf("[3.5] Setting output size to %dx%d...\r\n", CAMERA_WIDTH, CAMERA_HEIGHT);
    OV5640_OutSize_Set(4, 0, CAMERA_WIDTH, CAMERA_HEIGHT);
    
    // 色调设置（参考探索者项目：ATK_MC5640_HUE_6 = 0度，用于修复颜色偏移问题）
    OV5640_Hue_Set(0);
    
    // printf("[OK] Camera parameters configured! Resolution: %dx%d\r\n", CAMERA_WIDTH, CAMERA_HEIGHT);
    
    // 4. 初始化烟雾传感器
    // printf("[4] Initializing Smoke Sensor...\r\n");
    Smoke_Sensor_Init();
    // printf("[OK] Smoke sensor initialized!\r\n");
    
    // 5. 初始化USART2（用于ESP32通信）
    // printf("[5] Initializing USART2 (communicate with ESP32)...\r\n");
    USART2_Init(115200);
    USART2_ConfigInterrupt();
    // printf("[OK] USART2 initialized!\r\n");
    
    // 6. 初始化ESP8266（WiFi通信）
    // printf("[6] Initializing ESP8266...\r\n");
    USART3_Init(115200);  // 初始化USART3，波特率115200
    USART3_ConfigInterrupt();  // 配置USART3中断
    ESP8266_Init();
    ESP8266_Report_Init();  // 初始化ESP8266报告模块
    // printf("[OK] ESP8266 initialized!\r\n");
    
    // 7. 初始化SD卡数据管理器（暂时禁用）
    // printf("[7] SD Card Data Manager disabled for now\r\n");
    
    // 8. 初始化DCMI（后初始化DCMI）
    // printf("[8] Initializing DCMI...\r\n");
    My_DCMI_Init();
    
    // 9. 启动捕获
    // printf("[9] Starting capture...\r\n");
    
    // 启动第一帧
    DCMI_StartOneFrame();
    
    // printf("\r\n[OK] System initialized! Monitoring...\r\n");
    // printf("========================================\r\n\r\n");
    
    // 时间变量
    u32 last_sensor_time = 0;         // 上次传感器采集时间
    u32 last_save_time = 0;            // 上次数据保存时间
    u32 last_daily_time = 0;           // 上次生成日报时间
    u32 last_cleanup_time = 0;         // 上次清理数据时间
    u32 last_status_time = 0;          // 上次打印状态时间
    
    // 帧计数器（用于控制火焰检测频率）
    static u32 frame_counter = 0;
    
    while(1)
    {
        u32 current_time = millis();
        
        // 调试：打印时间值
        static u32 last_debug_time = 0;
        if(current_time - last_debug_time >= 1000)  // 每1秒打印一次
        {
            last_debug_time = current_time;
            printf("[DEBUG] current_time=%lu, last_sensor_time=%lu, diff=%lu\r\n", 
                   current_time, last_sensor_time, current_time - last_sensor_time);
        }
        
        // ----- 1. 每1秒打印一次状态 ----- (暂时禁用，避免干扰JPEG数据传输)
        // if(current_time - last_status_time >= 1000)
        // {
        //     last_status_time = current_time;
        //     printf("[Status] Frames: %d, Dropped: %d, Raw: %d, Processed: %d, Records: %d, Alerts: %d\r\n", 
        //            jpeg_serial_get_frame_count(), 
        //            jpeg_serial_get_dropped_frames(),
        //            g_raw_frame_count,
        //            g_processed_frame_count,
        //            g_record_count,
        //            g_fire_alert_count + g_smoke_alert_count);
        // }
        
        // ----- 2. 传感器数据采集和发送（每5秒） ----- 
        if(current_time - last_sensor_time >= SENSOR_INTERVAL)
        {
            last_sensor_time = current_time;
            
            // 发送传感器数据到ESP8266
            ESP8266_Report_SendSensorData();
        }
        
        // ----- 3. 图像采集和处理 -----
        // 直接调用process_jpeg_frame，让函数内部检查缓冲区状态
        static u32 last_process_time = 0;
        if(current_time - last_process_time >= 100) {  // 每100ms处理一次
            last_process_time = current_time;
            
            printf("[MAIN] Calling process_jpeg_frame\r\n");
            
            // 处理当前帧数据
            frame_counter++;
            
            // 判断是否需要进行火焰检测
            u8 do_fire_detection = 0;
            if(frame_counter >= FIRE_DETECT_INTERVAL) {
                do_fire_detection = 1;
                frame_counter = 0;  // 重置计数器
            }
            
            // 处理JPEG帧（根据参数决定是否进行火焰检测）
            //process_jpeg_frame(do_fire_detection);
						process_jpeg_frame(0);
        }
        
        // ----- 4. 数据保存（暂时禁用，数据通过ESP8266发送） -----
        // if(current_time - last_save_time >= DATA_SAVE_INTERVAL)
        // {
        //     last_save_time = current_time;
        //     g_record_count++;
        // }
        
        // ----- 5. 生成日报（暂时禁用） -----
        // if(current_time - last_daily_time >= DAILY_REPORT_INTERVAL)
        // {
        //     last_daily_time = current_time;
        //     generate_daily_report();
        // }
        
        // ----- 6. 清理过期数据（暂时禁用） -----
        // if(current_time - last_cleanup_time >= CLEANUP_INTERVAL)
        // {
        //     last_cleanup_time = current_time;
        //     SD_DataManager_CleanupOldData();
        // }
        
        // ----- 7. 检查SD卡空间（暂时禁用） -----
        // u8 space_status = SD_DataManager_CheckSpace();
        // if(space_status == 2)  // 空间严重不足
        // {
        //     printf("[WARNING] SD card space critical! Performing emergency cleanup...\r\n");
        //     SD_DataManager_EmergencyCleanup();
        // }
        // else if(space_status == 1)  // 空间不足
        // {
        //     printf("[WARNING] SD card space low!\r\n");
        // }
        
        // ----- 8. 检查USART DMA发送是否完成 ----- 
        if(usart1_dma_complete)
        {
            // 释放发送完成的缓冲区
            if(current_send_buf != NULL) {
                current_send_buf->state = BUF_IDLE;
                current_send_buf = NULL;
							// 清除完成标志
            usart1_dma_complete = 0;
            }
            
            
            
        }
        
        delay_ms(1);
    }
}

