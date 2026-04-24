/**
 * @file    usart3.h
 * @brief   USART3 底层通信驱动接口
 * @note    服务于 ESP8266 Wi-Fi 模块与云端服务器的 AT 交互及数据上传链路。
 */
#ifndef __USART3_H
#define __USART3_H

#include "sys.h"
#include <stdio.h>

#define USART3_RX_BUF_SIZE  256

void USART3_Init(u32 bound);
void USART3_ConfigInterrupt(void);
void USART3_Send_String(char* str);
void USART3_Send_Data(u8* data, u16 len);

extern u8  USART3_RX_BUF[USART3_RX_BUF_SIZE];
extern u16 USART3_RX_STA;
extern volatile u8 g_esp8266_upload_status;

#endif /* __USART3_H */
