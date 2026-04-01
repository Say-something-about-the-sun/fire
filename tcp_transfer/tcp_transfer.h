#ifndef __TCP_TRANSFER_H
#define __TCP_TRANSFER_H

#include "stm32f4xx.h"

// 电脑端的配置
#define PC_IP        "192.168.0.101"
#define PC_PORT      8080

// 函数声明
void TCP_Transfer_Init(void);
int  TCP_Transfer_Connect(void);
void TCP_Transfer_SendFrame(u8 *buf, u32 len);
void TCP_Transfer_Close(void);

#endif
