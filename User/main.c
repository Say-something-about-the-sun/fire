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
#include  "water_pump.h"
#include "key.h"
#include "ethernet_report.h"  // 引入网口备用链路



#include <stdarg.h>
// 引入 FreeRTOS 头文件
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"



//lwip
#include "malloc.h"




// 1. AI 检测结果结构体 (⚠️ 注意：如果你的结构体不叫 FireResult_t，请换成你真实的类型名)
extern FireDetectionResult g_latest_fire_result; 

// 2. 系统状态标志位
extern u8 g_system_mode;
extern u8 g_main_power_status;

// 3. 虚拟电流 (通常是 float 类型，如果是别的类型请自行修改)
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
        // 只要 DMA (假设 USART1_TX 用的是 DMA2_Stream7) 还在工作，就挂起等待，绝不插嘴！
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
    
	
	  Extsram_Init();
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
		
		//水泵初始化
		WaterPump_Init();
		
		
		// 唤醒 LwIP 协议栈
my_mem_init(SRAMIN);  

// 2. 捕获真实的错误码，并将延时换成裸机延时
u8 lwip_state;
printf("开始唤醒 LwIP...\r\n");

while((lwip_state = lwip_comm_init()) != 0) 
{
    printf("LwIP 初始化失败! 错误码: %d\r\n", lwip_state);
    
    // 🚨 操作系统启动前，绝对不能用 vTaskDelay！必须用原生的裸机延时！
    delay_ms(1000); 
}
printf("LwIP 唤醒成功! 网卡已就绪!\r\n");

   
    // 4. 初始化烟雾传感器
    Smoke_Sensor_Init();
    
		
		
		//温湿度初始化
		DHT11_Init();
		KEY_Init();
		
		
		
    // 5. 初始化USART2（用于ESP32通信）
    
    USART2_Init(115200);
    USART2_ConfigInterrupt();
    
    // 6. 初始化ESP8266（WiFi通信）
    USART3_Init(9600);  // 初始化USART3，波特率9600
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
		
	
	
	// 创建这把互斥锁
    Mutex_USART1 = xSemaphoreCreateMutex();
	
    
    // 2. 创建【启动任务】 (分配 256 字节栈，优先级最低设为 1)
    xTaskCreate((TaskFunction_t )start_task,            
                (const char* )"start_task",          
                (uint16_t       )1024,                   
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


// 声明外部的按键扫描任务函数 (实现在 esp8266_report.c 中)
extern void button_scan_task(void *pvParameters);

// 声明按键任务的句柄
TaskHandle_t ButtonTask_Handler;

// 【启动任务】：只负责把干活的任务创建出来，然后自杀
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL(); // 进入临界区，禁止中断打断任务创建

    // 创建图像处理任务：给足 4KB 栈(1024*4)，中等优先级 2
    xTaskCreate(camera_task, "CameraTask", 1024, NULL, 2, &CameraTask_Handler);
    
    // 创建上报任务：给足 4KB 栈(1024*4)，最高优先级 3（保证准时发送，不被图像卡住）
    xTaskCreate(report_task, "ReportTask", 512, NULL, 3, &ReportTask_Handler);

		// 3. 创建极速按键扫描任务：最高优先级 4
    // 扫按键只需要几微秒，极少占用CPU。给它最高优先级，保证它每 20ms 绝对能打断摄像头，
    xTaskCreate(button_scan_task, "ButtonTask", 128, NULL, 4, &ButtonTask_Handler);
	
    vTaskDelete(StartTask_Handler); // 任务全部创建完毕，删除自己
    
    taskEXIT_CRITICAL(); // 退出临界区
}

// 【图像处理任务】：取代了原来的 100ms 计时器
void camera_task(void *pvParameters)
{
    static u32 frame_counter = 0;
    static u8 fire_latch_counter = 0; // 🚀 搬到这里！水泵防抖锁定器
    
    while(1)
    {
        // 1. 获取 ESP32 数据 (包含了烟雾、红外火焰等状态)
        USART2_Process_ESP32_Data();
        
        // 2. 图像识别
        frame_counter++;
        u8 do_fire_detection = (frame_counter >= 5) ? 1 : 0;
        if(frame_counter >= 5) frame_counter = 0; 
        process_jpeg_frame(do_fire_detection);
        
        // ==============================================================
        // 💦 核心硬件条件反射中枢 (每 100ms 极速响应！)
        // ==============================================================
        // 只要本地AI看到火，就拉满锁定器
        
       u8 is_fire_real = (g_latest_fire_result.fire_detected == 1) /* || (ESP32_Flame_Status == 1) */;
        
        if (is_fire_real) {
            fire_latch_counter = 20; // 只要有火，强制拉满 20 帧 (2秒喷水延时)
        }

        
        
        if (g_system_mode == 0) // 自动模式下才允许控制硬件
        {
            // 🚨 【最高优先级】：全局过流保护红线！(独立于火灾状态)
            if (g_virtual_current > 10.0) 
            {
                g_main_power_status = 0;   // 虚拟跳闸，切断主电！
                WaterPump_Off();           // 禁水防触电！
                
                // 即使过载停水，火灾计时器也应当继续流逝
                if (fire_latch_counter > 0) {
                    fire_latch_counter--; 
                }
            }
            // ✅ 【第二优先级】：供电安全的情况下的消防联动
            else 
            {
                if (fire_latch_counter > 0) 
                {
                    fire_latch_counter--;  // 每 100ms 消耗 1
                    WaterPump_On();        // 💦 安全状态，允许持续喷水！
                } 
                else 
                {
                    WaterPump_Off();       // 🛑 锁定器归零，彻底停水
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
        
        // OS 延时 100 毫秒
        vTaskDelay(100);
    }
}


// 【传感器上报任务】：取代了原来的 5 秒计时器
void report_task(void *pvParameters)
{
    u8 link_status = 1; 

    while(1)
    {
        // 1. 尝试使用主链路 (WiFi) 上报数据，并阻塞等待回执 (最多等3秒)
        link_status = ESP8266_Report_SendSensorData();
        
        // 2. 故障转移机制 (Failover)
        if(link_status == 0)
        {
            printf("\r\n[WARN] 主链路(WiFi)失效，触发故障转移机制！\r\n");
            
            // 3. 瞬间激活备用网口链路，数据绝不丢失！
            // 这里执行完大约会耗时 0.2 ~ 0.5 秒
            Ethernet_Report_SendSensorData(); 
        }
        else
        {
            printf("\r\n[OK] 系统主备链路状态良好。\r\n");
        }
        
        // 4. 休眠 5 秒，等待下一次采集
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

