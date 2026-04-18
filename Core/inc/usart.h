#ifndef __USART_H
#define __USART_H
#include "stdio.h"	
#include "stm32f4xx_conf.h"
#include "sys.h" 
//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ���������ɣ��������������κ���;
//����1��ʼ��		   
//STM32F4����-�⺯���汾
//�Ա����̣�http://mcudev.taobao.com
//********************************************************************************

////////////////////////////////////////////////////////////////////////////////// 	
#define USART_REC_LEN  			200  	//�����������ֽ��� 200
#define EN_USART1_RX 			1		//ʹ�ܣ�1��/��ֹ��0������1����
	  	
extern u8  USART_RX_BUF[USART_REC_LEN]; //���ջ���,���USART_REC_LEN���ֽ�.ĩ�ֽ�Ϊ���з� 
extern u16 USART_RX_STA;         		//����״̬���	
//����봮���жϽ��գ��벻Ҫע�����º궨��
void uart_init(u32 bound);

void uart_send_byte(uint8_t byte);

// USART1 DMA发送相关函数
void usart1_dma_init(void);
void usart1_dma_send(uint8_t* data, uint16_t len);

// USART1 DMA发送完成标志（外部访问）
extern volatile uint8_t usart1_dma_complete;


void Safe_Printf(char *format, ...);

#endif


