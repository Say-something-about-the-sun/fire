#include "key.h"
#include "delay.h"

/**
 * @brief  初始化按键
 */
/**
 * @brief  初始化按键
 */
void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 1. 使能所有按键对应的 GPIO 时钟
    // 把所有的 RCC 宏用按位或 (|) 连起来，一行搞定！
    RCC_AHB1PeriphClockCmd(KEY0_RCC | KEY1_RCC | KEY2_RCC | WKUP_RCC, ENABLE);

    // 2. 初始化 KEY0, KEY1, KEY2
    // 因为这三个按键都在 GPIOE 上，而且都是低电平有效，所以可以合并在一起初始化
    GPIO_InitStructure.GPIO_Pin = KEY0_PIN | KEY1_PIN | KEY2_PIN; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; // 另一端接地，必须配置为内部上拉
    GPIO_Init(KEY0_PORT, &GPIO_InitStructure);   // 既然端口一样，传 KEY0_PORT 代表即可

    // 3. 单独初始化 WK_UP
    // 因为它在 GPIOA 上，且是高电平有效，必须单独配置为下拉
    GPIO_InitStructure.GPIO_Pin = WKUP_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN; // 另一端接 3.3V，必须配置为内部下拉
    GPIO_Init(WKUP_PORT, &GPIO_InitStructure);
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
