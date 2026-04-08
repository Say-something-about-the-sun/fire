#ifndef __USART3_H
#define __USART3_H

#include "sys.h"
#include <stdio.h>

// 缓冲区大小
#define USART3_RX_BUF_SIZE  256

// 函数声明
void USART3_Init(u32 bound);
void USART3_ConfigInterrupt(void);
void USART3_Send_String(char* str);
void USART3_Send_Data(u8* data, u16 len);

// 外部变量
extern u8 USART3_RX_BUF[USART3_RX_BUF_SIZE];
extern u16 USART3_RX_STA;
extern volatile u8 g_esp8266_upload_status;

#endif /* __USART3_H */
