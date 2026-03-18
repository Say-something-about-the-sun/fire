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

// 引入 FreeRTOS 头文件
#include "FreeRTOS.h"
#include "task.h"


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

/* ================== FreeRTOS 任务句柄与函数声明 ================== */
TaskHandle_t StartTask_Handler;
TaskHandle_t CameraTask_Handler;
TaskHandle_t ReportTask_Handler;

void start_task(void *pvParameters);
void camera_task(void *pvParameters);
void report_task(void *pvParameters);
/* ==================================== */

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
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    delay_init(168);
    uart_init(921600);
    
    // 初始化LED
    LED_Init();
    
    // printf("\r\n========================================\r\n");
    // printf("火灾监测系统 - 完整集成版本\r\n");
    // printf("========================================\r\n\r\n");
    
    // 1. 初始化RTC
    RTC_Init_Custom();
    // 2. 初始化JPEG和串口系统
    jpeg_serial_init();
    
    // 3. 初始化摄像头
    // printf("[3] Initializing OV5640...\r\n");
    if( OV5640_Init() != 0) {
        printf("[ERROR] OV5640 initialization failed!\r\n");
        while(1);
    }
    
    // printf("[3.5] Setting output size to %dx%d...\r\n", CAMERA_WIDTH, CAMERA_HEIGHT);
    OV5640_OutSize_Set(4, 0, CAMERA_WIDTH, CAMERA_HEIGHT);
    // 色调设置
    OV5640_Hue_Set(0);
   
    // 4. 初始化烟雾传感器
    Smoke_Sensor_Init();
    
    // 5. 初始化USART2（用于ESP32通信）
    
    USART2_Init(115200);
    USART2_ConfigInterrupt();
    
    // 6. 初始化ESP8266（WiFi通信）
    USART3_Init(115200);  // 初始化USART3，波特率115200
    USART3_ConfigInterrupt();  // 配置USART3中断
    ESP8266_Init();
    ESP8266_Report_Init();  // 初始化ESP8266报告模块
    
    
    // 7. 初始化SD卡数据管理器（暂时禁用）
    
    // 8. 初始化DCMI（后初始化DCMI）
    
    My_DCMI_Init();
    
    // 9. 启动捕获
    
    // 启动第一帧
    DCMI_StartOneFrame();
    
    // printf("\r\n[OK] System initialized! Monitoring...\r\n");
    // printf("========================================\r\n\r\n");
    printf("[OK] Hardware initialized! Starting FreeRTOS Scheduler...\r\n");
		
	
    
    // 2. 创建【启动任务】 (分配 256 字节栈，优先级最低设为 1)
    xTaskCreate((TaskFunction_t )start_task,            
                (const char* )"start_task",          
                (uint16_t       )256,                   
                (void* )NULL,                  
                (UBaseType_t    )1,                     
                (TaskHandle_t* )&StartTask_Handler);   

    // 3. 开启 FreeRTOS 任务调度器！系统从此由 OS 接管！
    vTaskStartScheduler();
    
    // 如果系统内存不足导致调度器启动失败，才会运行到这里
    while(1) {
        printf("[ERROR] FreeRTOS Scheduler failed to start! Check Memory.\r\n");
        delay_ms(1000);
    }
}

/* ================== FreeRTOS 任务实现 ================== */

// 【启动任务】：只负责把干活的任务创建出来，然后自杀
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL(); // 进入临界区，禁止中断打断任务创建

    // 创建图像处理任务：给足 4KB 栈(1024*4)，中等优先级 2
    xTaskCreate(camera_task, "CameraTask", 1024, NULL, 2, &CameraTask_Handler);
    
    // 创建上报任务：给足 4KB 栈(1024*4)，最高优先级 3（保证准时发送，不被图像卡住）
    xTaskCreate(report_task, "ReportTask", 1024, NULL, 3, &ReportTask_Handler);

    vTaskDelete(StartTask_Handler); // 任务全部创建完毕，删除自己
    
    taskEXIT_CRITICAL(); // 退出临界区
}

// 【图像处理任务】：取代了原来的 100ms 计时器
void camera_task(void *pvParameters)
{
    static u32 frame_counter = 0;
    
    while(1)
    {
        // 1. 处理图像帧
        frame_counter++;
        
        // 判断是否需要进行火焰检测 (如果你想开启，传入1，目前按你代码传入0)
        u8 do_fire_detection = 0;
        /* if(frame_counter >= FIRE_DETECT_INTERVAL) {
            do_fire_detection = 1;
            frame_counter = 0;
        } 
        */
        
        // printf("[Camera Task] Processing JPEG frame...\r\n");
        process_jpeg_frame(0); 
        
        // 2. 检查 USART DMA 发送是否完成
        if(usart1_dma_complete)
        {
            if(current_send_buf != NULL) {
                current_send_buf->state = BUF_IDLE;
                current_send_buf = NULL;
                usart1_dma_complete = 0;
            }
        }
        
        // 3. OS 延时 100 毫秒（挂起当前任务，把 CPU 让给别的任务！）
        vTaskDelay(100);
    }
}

// 【传感器上报任务】：取代了原来的 5 秒计时器
void report_task(void *pvParameters)
{
    while(1)
    {
        // 1. 采集并发送传感器数据
        // printf("[Report Task] Wake up to send data!\r\n");
        ESP8266_Report_SendSensorData();
        
        // 2. 发送完毕后，任务进入休眠 5000 毫秒！
        // 这 5 秒内，这个任务绝对不会占用哪怕 0.001% 的 CPU！
        vTaskDelay(5000);
    }
}
