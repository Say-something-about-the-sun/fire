/**
 * @file    delay.h
 * @brief   系统延时与时基驱动接口
 */
#ifndef __DELAY_H
#define __DELAY_H                
#include "sys.h"

void delay_init(u8 SYSCLK);
void delay_ms(u32 nms);
void delay_us(u32 nus);

u32  millis(void);

#endif /* __DELAY_H */
