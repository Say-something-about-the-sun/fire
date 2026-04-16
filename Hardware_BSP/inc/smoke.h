#ifndef __SMOKE_H
#define __SMOKE_H

#include "stm32f4xx.h"
#include "stm32f4xx_adc.h"

// MQ-2烟雾传感器接口（使用ADC2，PA1引脚）
void Smoke_Sensor_Init(void);              // 烟雾传感器初始化（PA1=AO模拟输出）
uint16_t Smoke_Get_ADC_Value(void);        // 获取PA1的ADC值（0-4095）
float Smoke_Get_Concentration(void);      // 获取烟雾浓度（PPM，需要校准）

#endif
