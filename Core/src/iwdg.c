#include "iwdg.h"

// 初始化独立看门狗
// prer: 分频数:0~7(只有低 3 位有效!)
// rlr: 重装载寄存器值:低 11 位有效
// 时间计算(大概): Tout=((4*2^prer)*rlr)/32 (ms)
void IWDG_Init(u8 prer, u16 rlr) 
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable); // 使能对寄存器 IWDG_PR 和 IWDG_RLR 的写操作
    IWDG_SetPrescaler(prer);                      // 设置 IWDG 预分频值
    IWDG_SetReload(rlr);                          // 设置 IWDG 重装载值
    IWDG_ReloadCounter();                         // 按照 IWDG 重装载寄存器的值重装载 IWDG 计数器
    IWDG_Enable();                                // 使能 IWDG
}

// 喂狗
void IWDG_Feed(void)
{   
    IWDG_ReloadCounter(); // 喂狗，重新计数
}
