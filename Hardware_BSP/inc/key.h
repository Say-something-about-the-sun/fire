/**
 * @file    key.h
 * @brief   独立按键底层驱动接口
 */
#ifndef __KEY_H
#define __KEY_H

#include "sys.h"

/* 探索者 F407 按键引脚定义 */
#define KEY0_PORT       GPIOE
#define KEY0_PIN        GPIO_Pin_4
#define KEY0_RCC        RCC_AHB1Periph_GPIOE

#define KEY1_PORT       GPIOE
#define KEY1_PIN        GPIO_Pin_3
#define KEY1_RCC        RCC_AHB1Periph_GPIOE

#define WKUP_PORT       GPIOA
#define WKUP_PIN        GPIO_Pin_0
#define WKUP_RCC        RCC_AHB1Periph_GPIOA

#define KEY2_PORT       GPIOE
#define KEY2_PIN        GPIO_Pin_2
#define KEY2_RCC        RCC_AHB1Periph_GPIOE

/* 硬件电平读取宏 */
#define KEY0_VAL        GPIO_ReadInputDataBit(KEY0_PORT, KEY0_PIN) // 低电平有效
#define KEY1_VAL        GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN) // 低电平有效
#define WKUP_VAL        GPIO_ReadInputDataBit(WKUP_PORT, WKUP_PIN) // 高电平有效
#define KEY2_VAL        GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN) // 低电平有效

/* 按键扫描返回值定义 */
#define KEY0_PRES       1
#define KEY1_PRES       2
#define WKUP_PRES       3
#define KEY2_PRES       4

void KEY_Init(void);
u8   KEY_Scan(u8 mode);

#endif /* __KEY_H */
