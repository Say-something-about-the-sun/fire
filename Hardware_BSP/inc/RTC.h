/**
 * @file    RTC.h
 * @brief   硬件 RTC (实时时钟) 驱动接口
 */
#ifndef __RTC_H
#define __RTC_H

#include "stm32f4xx.h"

/* 外部调用接口声明 */
void RTC_Init_Custom(void);           // RTC 硬件初始化与配置
void RTC_Set_Time(char* time_str);    // 设置系统时间 (格式: YYYY-MM-DD HH:MM:SS)
void RTC_Get_Time(char* time_str);    // 获取当前时间字符串

#endif /* __RTC_H */
