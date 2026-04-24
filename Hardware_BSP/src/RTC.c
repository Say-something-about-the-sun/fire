/**
 * @file    RTC.c
 * @brief   RTC功能实现
 * @note    基于LSI内部低速时钟，确保持续的系统时间基准。
 */
#include "RTC.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_rtc.h"
#include "stm32f4xx_pwr.h"
#include <stdio.h>

RTC_TimeTypeDef RTC_TimeStruct;
RTC_DateTypeDef RTC_DateStruct;

void RTC_Init_Custom(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    
    RCC_LSICmd(ENABLE);
    while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);

    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    RCC_RTCCLKCmd(ENABLE);
    
    RTC_WaitForSynchro();
    
    RTC_InitTypeDef RTC_InitStructure;
    RTC_InitStructure.RTC_AsynchPrediv = 0x7F;
    RTC_InitStructure.RTC_SynchPrediv  = 0xFF;
    RTC_InitStructure.RTC_HourFormat   = RTC_HourFormat_24;
    RTC_Init(&RTC_InitStructure);
    
    /* 设置默认初始时间: 2026-01-01 00:00:00 */
    RTC_TimeStruct.RTC_Hours   = 0;
    RTC_TimeStruct.RTC_Minutes = 0;
    RTC_TimeStruct.RTC_Seconds = 0;
    RTC_SetTime(RTC_Format_BIN, &RTC_TimeStruct);
    
    RTC_DateStruct.RTC_Year    = 26;
    RTC_DateStruct.RTC_Month   = 1;
    RTC_DateStruct.RTC_Date    = 1;
    RTC_DateStruct.RTC_WeekDay = RTC_Weekday_Thursday;
    RTC_SetDate(RTC_Format_BIN, &RTC_DateStruct);
    
    RTC_WaitForSynchro();
    PWR_BackupAccessCmd(DISABLE);
}

/**
 * @brief  更新系统时间
 * @param  time_str 输入格式 "YYYY-MM-DD HH:MM:SS"
 */
void RTC_Set_Time(char* time_str)
{
    int year, month, day, hour, minute, second;
    sscanf(time_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
    
    PWR_BackupAccessCmd(ENABLE);
    
    RTC_DateStruct.RTC_Year  = year % 100;
    RTC_DateStruct.RTC_Month = month;
    RTC_DateStruct.RTC_Date  = day;
    RTC_SetDate(RTC_Format_BIN, &RTC_DateStruct);
    
    RTC_TimeStruct.RTC_Hours   = hour;
    RTC_TimeStruct.RTC_Minutes = minute;
    RTC_TimeStruct.RTC_Seconds = second;
    RTC_SetTime(RTC_Format_BIN, &RTC_TimeStruct);
    
    RTC_WaitForSynchro();
    PWR_BackupAccessCmd(DISABLE);
}

/**
 * @brief  获取当前系统时间字符串
 * @param  time_str 输出缓冲区
 */
void RTC_Get_Time(char* time_str)
{
    RTC_WaitForSynchro();
    RTC_GetTime(RTC_Format_BIN, &RTC_TimeStruct);
    RTC_GetDate(RTC_Format_BIN, &RTC_DateStruct);
    
    sprintf(time_str, "20%02d-%02d-%02d %02d:%02d:%02d",
            RTC_DateStruct.RTC_Year, RTC_DateStruct.RTC_Month,
            RTC_DateStruct.RTC_Date, RTC_TimeStruct.RTC_Hours,
            RTC_TimeStruct.RTC_Minutes, RTC_TimeStruct.RTC_Seconds);
}
