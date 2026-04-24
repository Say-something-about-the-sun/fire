/**
 * @file    iwdg.c
 * @brief   独立看门狗驱动实现
 * @note    用于系统异常状态下的硬件复位保护。
 */
#include "iwdg.h"

/**
 * @brief  初始化独立看门狗
 * @param  prer 预分频数:0~7 (仅低 3 位有效)
 * @param  rlr  重装载寄存器值: (低 11 位有效)
 * @note   溢出时间计算(近似): Tout = ((4 * 2^prer) * rlr) / 32 (ms)
 */
void IWDG_Init(u8 prer, u16 rlr) 
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable); /* 使能对寄存器 IWDG_PR 和 IWDG_RLR 的写操作 */
    IWDG_SetPrescaler(prer);                      /* 设置 IWDG 预分频值 */
    IWDG_SetReload(rlr);                          /* 设置 IWDG 重装载值 */
    IWDG_ReloadCounter();                         /* 将重装载寄存器的值载入计数器 */
    IWDG_Enable();                                /* 使能 IWDG */
}

/**
 * @brief  喂狗操作
 * @note   重置看门狗计数器，防止系统复位。
 */
void IWDG_Feed(void)
{   
    IWDG_ReloadCounter(); 
}
