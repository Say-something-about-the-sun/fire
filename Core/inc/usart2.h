/**
 * @file    usart2.h
 * @brief   USART2 底层通信接口
 * @note    负责管理与外部模块 (如 ESP32) 的传感器数据同步。
 */
#ifndef __USART2_H
#define __USART2_H

#include "sys.h"

/* USART2 引脚分配 */
#define USART2_TX_PIN GPIO_Pin_2
#define USART2_RX_PIN GPIO_Pin_3
#define USART2_TX_PORT GPIOA
#define USART2_RX_PORT GPIOA

/**
 * @brief ESP32 传感器数据结构体
 */
typedef struct {
    char ntp_time[25];    /* NTP同步时间字符串 */
    u8   time_updated;    /* 时间更新状态标识 */
    u16  flame_adc;       /* 红外模拟信号值 */
    u8   flame_do;        /* 红外数字阈值状态 */
    u8   fire_detected;   /* ESP32综合火灾报警标志 */
} ESP32_SensorData;

/* 接收缓冲区配置 */
#define ESP32_RX_BUF_SIZE 128
extern u8 ESP32_RX_BUF[ESP32_RX_BUF_SIZE];
extern u16 ESP32_RX_STA;
extern ESP32_SensorData g_esp32_data;

/* 驱动接口声明 */
void USART2_Init(u32 baudrate);
void USART2_ConfigInterrupt(void);
void USART2_Send_Byte(u8 data);
void USART2_Send_String(char* str);
void USART2_Process_ESP32_Data(void);

#endif /* __USART2_H */
