/**
 * @file    led.c
 * @brief   状态指示灯驱动实现
 */
#include "led.h"

/**
 * @brief  LED 引脚初始化
 */
void LED_Init(void)
{
    // 初始化PF9/PF10 (开发板上与LED灯相连)
    GPIO_InitTypeDef GPIO_InitStruct;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;  
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOF, &GPIO_InitStruct);
}

/**
 * @brief  点亮指定 LED
 * @param  ch LED 标识字符串
 */
void led_on(char *ch)
{
    if(strcmp(ch, led0) == 0)
        GPIO_ResetBits(GPIOF, GPIO_Pin_9);
    if(strcmp(ch, led1) == 0)
        GPIO_ResetBits(GPIOF, GPIO_Pin_10);
}

/**
 * @brief  熄灭指定 LED
 * @param  ch LED 标识字符串
 */
void led_off(char *ch)
{
    if(strcmp(ch, led0) == 0)
        GPIO_SetBits(GPIOF, GPIO_Pin_9);
    if(strcmp(ch, led1) == 0)
        GPIO_SetBits(GPIOF, GPIO_Pin_10);
}
