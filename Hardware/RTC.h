#ifndef __RTC_H
#define __RTC_H

#include "stm32f4xx.h"

void RTC_Init_Custom(void);
void RTC_Set_Time(char* time_str);
void RTC_Get_Time(char* time_str);

#endif

