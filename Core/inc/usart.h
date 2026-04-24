/**
 * @file    usart.h
 * @brief   USART1 底层驱动接口
 * @note    支持 printf 重定向与 DMA 数据发送（用于高速图像传输）。
 */
#ifndef __USART_H
#define __USART_H
#include "stdio.h"  
#include "stm32f4xx_conf.h"
#include "sys.h" 

#define USART_REC_LEN           200     // 接收缓冲区最大长度
#define EN_USART1_RX            1       // 使能(1)/禁止(0) 串口1接收逻辑
        
extern u8  USART_RX_BUF[USART_REC_LEN]; // 接收缓冲数组
extern u16 USART_RX_STA;                // 接收状态寄存位

void uart_init(u32 bound);
void uart_send_byte(uint8_t byte);

/* USART1 DMA 发送控制接口 */
void usart1_dma_init(void);
void usart1_dma_send(uint8_t* data, uint16_t len);

/* USART1 DMA 发送完成全局标识 */
extern volatile uint8_t usart1_dma_complete;

void Safe_Printf(char *format, ...);

#endif /* __USART_H */
