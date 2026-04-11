// main.c - 主程序 - 战略降级稳定版 (MVP)
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
#include  "water_pump.h"
#include "key.h"
#include "ethernet_report.h"  // 引入网口备用链路 (保留头文件以防编译报错)

#include <stdarg.h>
// 引入 FreeRTOS 头文件
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

//lwip
#include "malloc.h"

// 定义一个全局变量：0-正常模式，1-强行禁用WiFi(演示用，当前降级版本中不再起决定性作用，仅保留防止外部报错)
u8 g_wifi_manual_off = 0;

// 1. AI 检测结果结构体
extern FireDetectionResult g_latest_fire_result; 

// 2. 系统状态标志位
extern u8 g_system_mode;
extern u8 g_main_power_status;

// 3. 虚拟电流
extern float g_virtual_current;

// 1. 定义一把全局串口互斥锁
SemaphoreHandle_t Mutex_USART1;

// 2. 打造工业级的安全打印函数
void Safe_Printf(char *format, ...)
{
    // 如果操作系统已经启动，才使用锁机制
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) 
    {
        // 拿锁：防止别的任务此时也想打印
        xSemaphoreTake(Mutex_USART1, portMAX_DELAY);
        
        // 🚨 【核心防爆逻辑】：等待底层的 DMA 图片发送彻底结束！
        while ((DMA2_Stream7->CR & DMA_SxCR_EN) != RESET) {
            vTaskDelay(1); 
        }
    }

    // 调用底层的字符格式化发送
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // 释放锁，让给别人用
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xSemaphoreGive(Mutex_USART1);
    }
}

// 摄像头参数
#define CAMERA_WIDTH  320
#define CAMERA_HEIGHT 240

// 系统参数
#define SENSOR_INTERVAL    5000    // 传感器采集间隔（ms）
#define DATA_SAVE_INTERVAL 1000    // 数据保存间隔（ms）
#define DAILY_REPORT_INTERVAL 86400000  // 日报生成间隔（24小时）
#define CLEANUP_INTERVAL   86400000  // 清理数据间隔（24小时）

// 外部变量声明
extern volatile u32 g_raw_frame_count;  
extern volatile u32 g_processed_frame_count;  
extern volatile u8 dma_transfer_complete;   
extern volatile BufferControl buf_ctrl_a;  
extern volatile BufferControl buf_ctrl_b;  

// 全局变量
SensorRecord g_current_record;      

// 统计变量
u32 g_record_count = 0;             
u32 g_fire_alert_count = 0;         
u32 g_smoke_alert_count = 0;        
u32 g_temp_alert_count = 0;         
u32 g_smoke_adc_sum = 0;            
u16 g_max_smoke_adc = 0;            
u16 g_max_flame_adc = 0;            
float g_temp_sum = 0;               
float g_max_temperature = 0;        

/* ================== FreeRTOS 任务句柄与函数声明 ================== */
TaskHandle_t StartTask_Handler;
TaskHandle_t CameraTask_Handler;
TaskHandle_t ReportTask_Handler;
TaskHandle_t ButtonTask_Handler;

void start_task(void *pvParameters);
void camera_task(void *pvParameters);
void report_task(void *pvParameters);
extern void button_scan_task(void *pvParameters);
/* ==================================== */

// OV5640色调设置函数
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
    
    SD_DataManager_GetCurrentDateString(report.date, sizeof(report.date));
    
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
    
    if(g_fire_alert_count > 0)
        report.max_risk_level = 3;
    else if(g_smoke_alert_count > 0)
        report.max_risk_level = 2;
    else if(g_temp_alert_count > 0)
        report.max_risk_level = 1;
    else
        report.max_risk_level = 0;
    
    SD_DataManager_SaveDailyReport(&report);
    
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
    
    // 1. 初始化RTC
    RTC_Init_Custom();
    // 2. 初始化JPEG和串口系统
    jpeg_serial_init();
    
    Extsram_Init();
    // 3. 初始化摄像头
    if( OV5640_Init() != 0) {
        printf("[ERROR] OV5640 initialization failed!\r\n");
    }
    
    OV5640_OutSize_Set(4, 0, CAMERA_WIDTH, CAMERA_HEIGHT);
    OV5640_Hue_Set(0);
        
    //水泵初始化
    WaterPump_Init();
        
    // ========================================================
    // 🔪 战略降级边界 1：隔离 LwIP 协议栈底层初始化
    // 保留了 SRAM 初始化给其他外设，彻底封死 MAC 硬件避免干扰
    // ========================================================
    my_mem_init(SRAMIN);  // 🚨 绝对保留

#if 0 
    u8 lwip_state;
    printf("开始唤醒 LwIP...\r\n");

    while((lwip_state = lwip_comm_init()) != 0) 
    {
        printf("LwIP 初始化失败! 错误码: %d\r\n", lwip_state);
        delay_ms(1000); 
    }
    printf("LwIP 唤醒成功! 网卡已就绪!\r\n");
#else
    printf("[SYS] LwIP & Ethernet PHY bypassed for MVP stability.\r\n");
#endif
    // ========================================================

    // 4. 初始化烟雾传感器
    Smoke_Sensor_Init();
        
    //温湿度初始化
    DHT11_Init();
    KEY_Init();
        
    // 5. 初始化USART2（用于ESP32通信）
    USART2_Init(115200);
    USART2_ConfigInterrupt();
    
    // 6. 初始化ESP8266（WiFi通信）
    USART3_Init(9600);  
    USART3_ConfigInterrupt();  
    ESP8266_Init();
    ESP8266_Report_Init();  
    
    // 8. 初始化DCMI（后初始化DCMI）
    My_DCMI_Init();
    
    // 9. 启动第一帧
    DCMI_StartOneFrame();
    
    printf("[OK] Hardware initialized! Starting FreeRTOS Scheduler...\r\n");
        
    // 创建这把互斥锁
    Mutex_USART1 = xSemaphoreCreateMutex();
    
    // 2. 创建【启动任务】 (分配 256 字节栈，优先级最低设为 1)
    xTaskCreate((TaskFunction_t )start_task,            
                (const char* )"start_task",          
                (uint16_t       )1024,                   
                (void* )NULL,                  
                (UBaseType_t    )1,                     
                (TaskHandle_t* )&StartTask_Handler);   

    // 3. 开启 FreeRTOS 任务调度器！
    vTaskStartScheduler();
    
    while(1) {
        printf("[ERROR] FreeRTOS Scheduler failed to start! Check Memory.\r\n");
        delay_ms(1000);
    }
}

/* ================== FreeRTOS 任务实现 ================== */

// 【启动任务】
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();

    // 创建图像处理任务
    xTaskCreate(camera_task, "CameraTask", 1024, NULL, 2, &CameraTask_Handler);
    
    // 创建上报任务
    xTaskCreate(report_task, "ReportTask", 1024, NULL, 3, &ReportTask_Handler);

    // 创建极速按键扫描任务
    xTaskCreate(button_scan_task, "ButtonTask", 128, NULL, 4, &ButtonTask_Handler);
    
    vTaskDelete(StartTask_Handler); 
    
    taskEXIT_CRITICAL(); 
}

// 【图像处理任务】
void camera_task(void *pvParameters)
{
    static u32 frame_counter = 0;
    static u8 fire_latch_counter = 0; 
    
    while(1)
    {
        // 1. 获取 ESP32 数据 
        USART2_Process_ESP32_Data();
        
        // 2. 图像识别
        frame_counter++;
        u8 do_fire_detection = (frame_counter >= 5) ? 1 : 0;
        if(frame_counter >= 5) frame_counter = 0; 
        process_jpeg_frame(do_fire_detection);
        
        // ==============================================================
        // 💦 核心硬件条件反射中枢
        // ==============================================================
        u8 is_fire_real = (g_latest_fire_result.fire_detected == 1);
        
        if (is_fire_real) {
            fire_latch_counter = 20; 
        }
        
        if (g_system_mode == 0) 
        {
            if (g_virtual_current > 10.0) 
            {
                g_main_power_status = 0;   
                WaterPump_Off();           
                
                if (fire_latch_counter > 0) {
                    fire_latch_counter--; 
                }
            }
            else 
            {
                if (fire_latch_counter > 0) 
                {
                    fire_latch_counter--;  
                    WaterPump_On();        
                } 
                else 
                {
                    WaterPump_Off();       
                }
            }
        }
        // ==============================================================
        
        // 3. 检查 USART DMA 发送是否完成
        if(usart1_dma_complete)
        {
            if(current_send_buf != NULL) {
                current_send_buf->state = BUF_IDLE;
                current_send_buf = NULL;
                usart1_dma_complete = 0;
            }
        }
        
        vTaskDelay(100);
    }
}


// 【传感器上报任务】：战略降级极简版
void report_task(void *pvParameters)
{
    while(1)
    {
        printf("\r\n--- Standard Reporting Cycle ---\r\n");

        // 1. 无视所有容灾状态机，强制且唯一执行 WiFi 上报
        u8 wifi_result = ESP8266_Report_SendSensorData(); 

        if (wifi_result == 0) 
        {
            printf("[WARN] ESP8266 Timeout/Error. Data discarded this cycle.\r\n");
            
// ========================================================
// 🔪 战略降级边界 2：彻底砍断以太网备用链路及复杂切换
// 确保即使 WiFi 失败，也不会强行接管总线导致摄像头或 CPU 死锁
// ========================================================
#if 0
            printf("\r\n[WARN] Triggering Ethernet failover...\r\n");
            Ethernet_Report_SendSensorData();
#endif
// ========================================================
        }
        else 
        {
            printf("[OK] Data pushed successfully via ESP8266.\r\n"); 
        }

        // 严格周期
        vTaskDelay(5000 / portTICK_PERIOD_MS); 
    }
}
