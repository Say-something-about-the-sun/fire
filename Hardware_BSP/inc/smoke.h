/**
 * @file    smoke.h
 * @brief   MQ-2 烟雾传感器底层驱动接口
 */
#ifndef __SMOKE_H
#define __SMOKE_H

#include "stm32f4xx.h"
#include "stm32f4xx_adc.h"

/* 外部调用接口声明 */
void     Smoke_Sensor_Init(void);          // 烟雾传感器初始化（ADC2, PA5引脚）
uint16_t Smoke_Get_ADC_Value(void);        // 获取 PA5 的 ADC 值（0-4095）
float    Smoke_Get_Concentration(void);    // 获取烟雾浓度（PPM）

#endif /* __SMOKE_H */
