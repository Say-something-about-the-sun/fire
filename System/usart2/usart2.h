// usart2.h - USART2头文件

#ifndef __USART2_H
#define __USART2_H

#include "sys.h"

// USART2引脚定义
#define USART2_TX_PIN GPIO_Pin_2
#define USART2_RX_PIN GPIO_Pin_3
#define USART2_TX_PORT GPIOA
#define USART2_RX_PORT GPIOA

// ESP32数据接收缓冲区定义
#define ESP32_RX_BUF_SIZE 128
extern u8 ESP32_RX_BUF[ESP32_RX_BUF_SIZE];
extern u16 ESP32_RX_STA;


typedef struct {
    char ntp_time[25];    // NTP时间字符串 (如 "2026-03-22 21:56:28")
    u8 time_updated;      // 时间更新标志
    
    u16 flame_adc;        // 火焰模拟量
    u8 flame_do;          // 火焰数字量 (0或1)
    u8 fire_detected;     // ESP32火焰综合报警标志
} ESP32_SensorData;



// 函数声明
void USART2_Init(u32 baudrate);
void USART2_ConfigInterrupt(void);
void USART2_Send_Byte(u8 data);
void USART2_Send_String(char* str);
void USART2_Process_ESP32_Data(void);

#endif /* __USART2_H */
