#ifndef __FLAME_H
#define __FLAME_H

#include "stm32f4xx.h"
#include "stm32f4xx_adc.h"

// 函数声明（无任何宏定义）
void Flame_Sensor_Init(void);    // 火焰传感器初始化（PA0=AO，PB0=DO）
uint16_t Flame_Get_ADC_Value(void); // 读取PA0的ADC值
uint8_t Flame_Get_DO_State(void);   // 读取PB0的数字状态

#endif
