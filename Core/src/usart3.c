/**
 * @file    usart3.c
 * @brief   USART3 底层通信驱动实现
 * @note    实现了针对云端服务器响应的高效字符串截获与解析。
 */
#include "usart3.h"
#include "delay.h"
#include <string.h>
#include "usart.h"

/* 全局网络状态标记: 0=空闲, 1=上报成功, 2=连接失败/断开 */
volatile u8 g_esp8266_upload_status = 0;

u8  USART3_RX_BUF[USART3_RX_BUF_SIZE];
u16 USART3_RX_STA = 0; 

/**
 * @brief  初始化 USART3 通信总线
 * @param  bound 波特率参数
 */
void USART3_Init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3); 
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_USART3); 

    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    USART_Cmd(USART3, ENABLE);

    Safe_Printf("[USART3] bound: %lu\r\n", bound);
    Safe_Printf("[USART3] : STM32 PB10 (TX) -> ESP8266 RX\r\n");
    Safe_Printf("[USART3] : STM32 PB11 (RX) -> ESP8266 TX\r\n");
}

void USART3_ConfigInterrupt(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    Safe_Printf("[USART3] Interrupt configured\r\n");
}

void USART3_Send_String(char* str)
{
    while(*str)
    {
        while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
        USART_SendData(USART3, *str++);
    }
    while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}

void USART3_Send_Data(u8* data, u16 len)
{
    u16 i;
    for(i = 0; i < len; i++)
    {
        while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
        USART_SendData(USART3, data[i]);
    }
    while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}

/**
 * @brief  USART3 接收中断服务
 * @note   集成云平台回执信息的非阻塞扫描机制。
 */
void USART3_IRQHandler(void)
{
    u8 Res;
    
    if(USART_GetFlagStatus(USART3, USART_FLAG_ORE) != RESET)
    {
        USART_ReceiveData(USART3); 
    }  
    
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) 
    {
        Res = USART_ReceiveData(USART3); 
        
        if((USART3_RX_STA & 0x8000) == 0) 
        {
            if(USART3_RX_STA & 0x4000) 
            {
                if(Res != 0x0a) {
                    USART3_RX_STA = 0; 
                } else {
                    USART3_RX_STA |= 0x8000; 
                                        
                    /* 数据防越界及云端回执扫描 */
                    USART3_RX_BUF[USART3_RX_STA & 0X3FFF] = '\0'; 
                    
                    if (strstr((const char*)USART3_RX_BUF, "UPLOAD_OK") != NULL) 
                    {
                        g_esp8266_upload_status = 1; 
                    }
                    else if (strstr((const char*)USART3_RX_BUF, "UPLOAD_ERR") != NULL || 
                             strstr((const char*)USART3_RX_BUF, "WIFI_ERR") != NULL) 
                    {
                        g_esp8266_upload_status = 2; 
                    }
                }
            }
            else 
            {
                if(Res == 0x0d) {
                    USART3_RX_STA |= 0x4000; 
                } else {
                    USART3_RX_BUF[USART3_RX_STA & 0X3FFF] = Res;
                    USART3_RX_STA++;
                    
                    if(USART3_RX_STA > (USART3_RX_BUF_SIZE - 1)) {
                        USART3_RX_STA = 0; 
                    }
                    
                    /* JSON 数据流扫描机制，拦截无回车换行的云端控制指令 */
                    if(Res == '}') 
                    {
                        USART3_RX_BUF[USART3_RX_STA] = '\0'; 
                        if(strstr((char*)USART3_RX_BUF, "pump_status") != NULL || 
                           strstr((char*)USART3_RX_BUF, "system_mode") != NULL) 
                        {
                            USART3_RX_STA |= 0x8000; 
                        }
                    }
                }
            }
        }
    }
    
    if(USART_GetFlagStatus(USART3, USART_FLAG_ORE) != RESET) 
    {
        USART_ReceiveData(USART3); 
    }
}
