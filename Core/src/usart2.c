/**
 * @file    usart2.c
 * @brief   USART2 底层通信驱动实现
 */
#include "usart2.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>
#include <RTC.h>

#include "FreeRTOS.h"
#include "semphr.h"

extern SemaphoreHandle_t Mutex_USART1;
void Safe_Printf(char *format, ...);

ESP32_SensorData g_esp32_data;

u8 ESP32_RX_BUF[ESP32_RX_BUF_SIZE];
u16 ESP32_RX_STA = 0;

/**
 * @brief  初始化 USART2 硬件接口
 * @param  baudrate 目标波特率
 */
void USART2_Init(u32 baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    
    GPIO_PinAFConfig(USART2_TX_PORT, GPIO_PinSource2, GPIO_AF_USART2); 
    GPIO_PinAFConfig(USART2_RX_PORT, GPIO_PinSource3, GPIO_AF_USART2); 
    
    GPIO_InitStructure.GPIO_Pin = USART2_TX_PIN | USART2_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStructure);
    
    USART_Cmd(USART2, ENABLE);
    USART_ClearFlag(USART2, USART_FLAG_TC);
}

/**
 * @brief  配置 USART2 中断服务策略
 */
void USART2_ConfigInterrupt(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void USART2_Send_Byte(u8 data)
{
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, data);
    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
}

void USART2_Send_String(char* str)
{
    if(str == 0) return;
    while(*str)
    {
        USART2_Send_Byte((u8)(*str));
        str++;
    }
}

/**
 * @brief  USART2 接收中断服务逻辑
 * @note   实现对协议帧尾分隔符 (\n 或 \r\n) 的有效拦截。
 */
void USART2_IRQHandler(void)
{
    u8 Res;
    
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        Res = USART_ReceiveData(USART2);
        
        if((ESP32_RX_STA & 0x8000) == 0) 
        {
            if(Res == 0x0a) 
            {
                ESP32_RX_STA |= 0x8000;
            }
            else if(ESP32_RX_STA & 0x4000) 
            {
                if(Res == 0x0a) 
                {
                    ESP32_RX_STA |= 0x8000;
                }
                else 
                {
                    ESP32_RX_STA = 0;
                }
            }
            else 
            {
                if(Res == 0x0d)
                {
                    ESP32_RX_STA |= 0x4000;
                }
                else
                {
                    ESP32_RX_BUF[ESP32_RX_STA & 0x3FFF] = Res;
                    ESP32_RX_STA++;
                    if(ESP32_RX_STA > (ESP32_RX_BUF_SIZE - 1))
                    {
                        ESP32_RX_STA = 0; 
                    }
                }
            }
        }
    }
    
    /* 清理 ORE 硬件溢出标志位，恢复数据总线 */
    if(USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) 
    {
        USART_ReceiveData(USART2); 
    }
}

/**
 * @brief  解析接收到的 ESP32 数据包
 * @note   对时间同步指令与环境传感器数据进行区分与转存。
 */
void USART2_Process_ESP32_Data(void)
{
    if(ESP32_RX_STA & 0x8000)
    {
        u16 len = ESP32_RX_STA & 0x3FFF;
        ESP32_RX_BUF[len] = '\0'; 
        
        char* str = (char*)ESP32_RX_BUF;
        
        if(strncmp(str, "TIME:", 5) == 0)
        {
            strncpy(g_esp32_data.ntp_time, str + 5, sizeof(g_esp32_data.ntp_time) - 1);
            g_esp32_data.ntp_time[sizeof(g_esp32_data.ntp_time) - 1] = '\0';
            g_esp32_data.time_updated = 1; 
            
            /* 核心逻辑：直接调用封装好的 RTC_Set_Time 函数进行时间同步 */
            RTC_Set_Time(str + 5); 
            
            USART2_Send_String("OK\r\n");
        }
        else if(strncmp(str, "FLAME_AO:", 9) == 0)
        {
            int ao = 0, do_val = 0, fire = 0;
            
            if(sscanf(str, "FLAME_AO:%d,FLAME_DO:%d,FIRE:%d", &ao, &do_val, &fire) == 3)
            {
                g_esp32_data.flame_adc = (u16)ao;
                g_esp32_data.flame_do = (u8)do_val;
                g_esp32_data.fire_detected = (u8)fire;
            }
            
            /* 通信握手机制：必须向 ESP32 回复应答信号，防止发送端阻塞等待 */
            USART2_Send_String("OK\r\n");
        }
        
        ESP32_RX_STA = 0;
        memset(ESP32_RX_BUF, 0, ESP32_RX_BUF_SIZE);
    }
}
