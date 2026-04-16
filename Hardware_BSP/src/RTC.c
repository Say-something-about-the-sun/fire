#include "RTC.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_rtc.h"
#include "stm32f4xx_pwr.h"
#include <stdio.h>

RTC_TimeTypeDef RTC_TimeStruct;
RTC_DateTypeDef RTC_DateStruct;

void RTC_Init_Custom(void)
{
    printf("[RTC] Initializing...\r\n");
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    
    printf("[RTC] PWR and Backup access enabled\r\n");
    
    // 使用LSI（内部低速振荡器）
RCC_LSICmd(ENABLE);
while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);
printf("[RTC] LSI ready\r\n");

// Enable RTC Clock
// RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
RCC_RTCCLKCmd(ENABLE);
    
    printf("[RTC] RTC clock enabled (LSE)\r\n");
    
    // Wait for RTC registers synchronization
    RTC_WaitForSynchro();
    
    printf("[RTC] RTC synchronized\r\n");
    
    // Initialize RTC
    RTC_InitTypeDef RTC_InitStructure;
    RTC_InitStructure.RTC_AsynchPrediv = 0x7F;
    RTC_InitStructure.RTC_SynchPrediv = 0xFF;
    RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24;
    RTC_Init(&RTC_InitStructure);
    
    printf("[RTC] RTC initialized\r\n");
    
    // Set default time: 2026-01-01 00:00:00
    RTC_TimeStruct.RTC_Hours = 0;
    RTC_TimeStruct.RTC_Minutes = 0;
    RTC_TimeStruct.RTC_Seconds = 0;
    RTC_SetTime(RTC_Format_BIN, &RTC_TimeStruct);
    
    RTC_DateStruct.RTC_Year = 26;
    RTC_DateStruct.RTC_Month = 1;
    RTC_DateStruct.RTC_Date = 1;
    RTC_DateStruct.RTC_WeekDay = RTC_Weekday_Thursday;
    RTC_SetDate(RTC_Format_BIN, &RTC_DateStruct);
    
    printf("[RTC] Default time set: 2026-01-01 00:00:00\r\n");
    
    // Wait for RTC registers synchronization
    RTC_WaitForSynchro();
    
    // Verify RTC is working by reading back the time
    RTC_GetTime(RTC_Format_BIN, &RTC_TimeStruct);
    RTC_GetDate(RTC_Format_BIN, &RTC_DateStruct);
    
    printf("[RTC] Verification: year=%d, month=%d, day=%d, hour=%d, minute=%d, second=%d\r\n", 
           RTC_DateStruct.RTC_Year, RTC_DateStruct.RTC_Month, RTC_DateStruct.RTC_Date,
           RTC_TimeStruct.RTC_Hours, RTC_TimeStruct.RTC_Minutes, RTC_TimeStruct.RTC_Seconds);
    
    PWR_BackupAccessCmd(DISABLE);
    printf("[RTC] Initialization completed\r\n");
}

void RTC_Set_Time(char* time_str)
{
    int year, month, day, hour, minute, second;
    sscanf(time_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    
    RTC_DateStruct.RTC_Year = year % 100;
    RTC_DateStruct.RTC_Month = month;
    RTC_DateStruct.RTC_Date = day;
    RTC_SetDate(RTC_Format_BIN, &RTC_DateStruct);
    
    RTC_TimeStruct.RTC_Hours = hour;
    RTC_TimeStruct.RTC_Minutes = minute;
    RTC_TimeStruct.RTC_Seconds = second;
    RTC_SetTime(RTC_Format_BIN, &RTC_TimeStruct);
    
    // Wait for RTC registers synchronization
    RTC_WaitForSynchro();
    
    PWR_BackupAccessCmd(DISABLE);
}

void RTC_Get_Time(char* time_str)
{
    // Wait for RTC time registers synchronization
    RTC_WaitForSynchro();
    
    RTC_GetTime(RTC_Format_BIN, &RTC_TimeStruct);
    RTC_GetDate(RTC_Format_BIN, &RTC_DateStruct);
    
    sprintf(time_str, "20%02d-%02d-%02d %02d:%02d:%02d",
            RTC_DateStruct.RTC_Year,
            RTC_DateStruct.RTC_Month,
            RTC_DateStruct.RTC_Date,
            RTC_TimeStruct.RTC_Hours,
            RTC_TimeStruct.RTC_Minutes,
            RTC_TimeStruct.RTC_Seconds);
}

