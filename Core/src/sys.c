/**
 * @file    sys.c
 * @brief   底层汇编指令封装实现
 */
#include "sys.h"  

/* THUMB指令不支持汇编内联，采用如下方法实现执行汇编指令WFI */
__asm void WFI_SET(void)
{
    WFI;          
}

/* 关闭所有中断 (不包括 fault 和 NMI 中断) */
__asm void INTX_DISABLE(void)
{
    CPSID   I
    BX      LR    
}

/* 开启所有中断 */
__asm void INTX_ENABLE(void)
{
    CPSIE   I
    BX      LR  
}

/* 设置主堆栈顶地址 */
__asm void MSR_MSP(u32 addr) 
{
    MSR MSP, r0             // set Main Stack value
    BX r14
}
