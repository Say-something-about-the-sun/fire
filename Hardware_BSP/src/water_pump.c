/**
 * @file    water_pump.c
 * @brief   水泵底层驱动实现
 * @note    维护水泵的物理开关状态，并执行底层的 GPIO 电平翻转。
 */
#include "water_pump.h"

/* 记录水泵当前物理状态：1=开，0=关 */
static u8 pump_status = 0;

/**
 * @brief  初始化水泵控制引脚
 * @param  无
 * @retval 无
 * @note   使用推挽输出，并配置下拉电阻防止上电瞬间悬空误触发
 */
void WaterPump_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 1. 使能 GPIO 时钟
    RCC_AHB1PeriphClockCmd(PUMP_RCC, ENABLE);

    // 2. 配置引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin   = PUMP_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    // 🚨 极其重要：配置下拉电阻！防止单片机刚上电复位时，引脚悬空导致误触发水泵喷水
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN; 
    GPIO_Init(PUMP_PORT, &GPIO_InitStructure);

    // 3. 初始状态：强制拉低关闭水泵
    PUMP_OFF();
    pump_status = 0;
}

/**
 * @brief  开启水泵
 */
void WaterPump_On(void) 
{
    PUMP_ON();
    pump_status = 1;
}

/**
 * @brief  关闭水泵
 */
void WaterPump_Off(void) 
{
    PUMP_OFF();
    pump_status = 0;
}

/**
 * @brief  获取当前水泵的物理状态
 * @param  无
 * @retval 1:运行中, 0:已关闭
 * @note   供 JSON 状态打包使用
 */
u8 WaterPump_GetStatus(void) 
{
    return pump_status;
}
