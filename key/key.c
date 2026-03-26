#include "key.h"
#include "delay.h"

/**
 * @brief  初始化按键
 */
void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 使能 GPIOE 和 GPIOA 时钟
    RCC_AHB1PeriphClockCmd(KEY0_RCC | WKUP_RCC, ENABLE);

    // 初始化 KEY0(PE4) 和 KEY1(PE3) 为上拉输入
    GPIO_InitStructure.GPIO_Pin = KEY0_PIN | KEY1_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; // 🚨 探索者的按键另一端接地，所以必须上拉
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    // 初始化 WK_UP(PA0) 为下拉输入
    GPIO_InitStructure.GPIO_Pin = WKUP_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN; // 🚨 WK_UP 另一端接 3.3V，必须下拉
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
 * @brief  按键扫描函数
 * @param  mode: 0=不支持连续按, 1=支持连续按
 * @retval 0=无按键, 1=KEY0, 2=KEY1, 3=WK_UP
 */
u8 KEY_Scan(u8 mode)
{
    static u8 key_up = 1; // 按键松开标志
    
    if (mode == 1) key_up = 1; // 支持连按
    
    if (key_up && (KEY0_VAL == 0 || KEY1_VAL == 0 || WKUP_VAL == 1))
    {
        delay_ms(10); // 去抖动
        key_up = 0;
        
        if (KEY0_VAL == 0) return KEY0_PRES;
        else if (KEY1_VAL == 0) return KEY1_PRES;
        else if (WKUP_VAL == 1) return WKUP_PRES;
    }
    else if (KEY0_VAL == 1 && KEY1_VAL == 1 && WKUP_VAL == 0)
    {
        key_up = 1;
    }
    
    return 0; // 无按键按下
}
