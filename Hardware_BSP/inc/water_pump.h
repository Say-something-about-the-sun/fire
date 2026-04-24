/**
 * @file    water_pump.h
 * @brief   水泵底层驱动接口
 * @note    提供基于 MOSFET 的水泵硬件控制接口。
 */
#ifndef __WATER_PUMP_H
#define __WATER_PUMP_H

#include "sys.h" // 包含 STM32 标准库引脚定义

/* 水泵控制引脚定义 (根据实际接线修改) */
#define PUMP_PORT       GPIOC
#define PUMP_PIN        GPIO_Pin_2
#define PUMP_RCC        RCC_AHB1Periph_GPIOC

/* 硬件控制宏 (高电平导通 MOSFET) */
#define PUMP_ON()       GPIO_SetBits(PUMP_PORT, PUMP_PIN)
#define PUMP_OFF()      GPIO_ResetBits(PUMP_PORT, PUMP_PIN)

/* 外部调用接口声明 */
void WaterPump_Init(void);
u8   WaterPump_GetStatus(void);
void WaterPump_On(void);
void WaterPump_Off(void);

#endif /* __WATER_PUMP_H */
