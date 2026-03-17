#ifndef __DELAY_H
#define __DELAY_H 			   
#include <sys.h>	  
//////////////////////////////////////////////////////////////////////////////////  
//STM32F407开发板
//STM32F4工程-库函数版本
//淘宝店铺：http://mcudev.taobao.com
//********************************************************************************
//修改说明
//无
////////////////////////////////////////////////////////////////////////////////// 	 
extern volatile u32 g_ms_ticks;

void delay_init(u8 SYSCLK);
void delay_ms(u16 nms);
void delay_us(u32 nus);


u32 millis(void);


#endif





























