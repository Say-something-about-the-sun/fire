/**
 * @file    key.c
 * @brief   独立按键底层驱动实现
 * @note    支持单次触发与长按连发模式。
 */
#include "key.h"
#include "delay.h"

/**
 * @brief  初始化所有按键引脚
 * @param  无
 * @retval 无
 */
void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 1. 批量使能 GPIO 时钟
    RCC_AHB1PeriphClockCmd(KEY0_RCC | KEY1_RCC | KEY2_RCC | WKUP_RCC, ENABLE);

    // 2. 初始化 KEY0, KEY1, KEY2 (低电平有效，需内部上拉)
    GPIO_InitStructure.GPIO_Pin   = KEY0_PIN | KEY1_PIN | KEY2_PIN; 
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP; 
    GPIO_Init(KEY0_PORT, &GPIO_InitStructure);   

    // 3. 初始化 WK_UP (高电平有效，需内部下拉)
    GPIO_InitStructure.GPIO_Pin   = WKUP_PIN;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN; 
    GPIO_Init(WKUP_PORT, &GPIO_InitStructure);
}

/**
 * @brief  按键扫描机制
 * @param  mode: 0 = 单次触发(不支持连按), 1 = 连续触发(支持连按)
 * @retval 0=无按键, 1=KEY0, 2=KEY1, 3=WK_UP, 4=KEY2
 */
u8 KEY_Scan(u8 mode)
{
    static u8 key_up = 1; // 按键松开标志位
    
    if (mode == 1) 
    {
        key_up = 1; // 连续触发模式，强行认为已松手
    }
    
    // 捕获按键按下瞬间
    if (key_up && (KEY0_VAL == 0 || KEY1_VAL == 0 || KEY2_VAL == 0 || WKUP_VAL == 1))
    {
        delay_ms(10); // 硬件去抖动
        key_up = 0;   // 标记按键已被按下
        
        if      (KEY0_VAL == 0) return KEY0_PRES;
        else if (KEY1_VAL == 0) return KEY1_PRES;
        else if (KEY2_VAL == 0) return KEY2_PRES; 
        else if (WKUP_VAL == 1) return WKUP_PRES;
    }
    // 捕获全员松手状态
    else if (KEY0_VAL == 1 && KEY1_VAL == 1 && KEY2_VAL == 1 && WKUP_VAL == 0)
    {
        key_up = 1;
    }
    
    return 0; // 无按键按下
}
