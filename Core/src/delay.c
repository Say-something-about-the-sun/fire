/**
 * @file    delay.c
 * @brief   系统延时与时基驱动实现
 * @note    支持裸机下的硬件阻塞延时，以及 FreeRTOS 环境下的任务挂起延时。
 */
#include "delay.h"
#include "sys.h"
#include "FreeRTOS.h"
#include "task.h"

static u32 fac_us = 0; /* 微秒延时倍乘数 */

/**
 * @brief  初始化延时函数
 * @param  SYSCLK 系统时钟频率 (STM32F4通常为 168)
 */
void delay_init(u8 SYSCLK)
{
    /* 配置 SysTick 时钟源为 HCLK */
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK); 
    fac_us = SYSCLK; 
    
    /* 系统死锁预防机制：在 FreeRTOS 接管前，强制启动滴答定时器。
       防止在 main() 前期执行的 delay_us/ms 陷入死循环。 */
    SysTick->LOAD = 0xFFFFFF; /* 填入最大重装载值 */
    SysTick->VAL = 0;         /* 清空当前计数值 */
    SysTick->CTRL |= 5;       /* 开启定时器 (BIT0=1) 并选择 HCLK (BIT2=1)，禁止中断 (BIT1=0) */
}

/**
 * @brief  微秒级延时 (硬件阻塞延时)
 * @param  nus 需要延时的微秒数
 * @note   纯硬件循环等待，不引发任务调度。
 */
void delay_us(u32 nus)
{
    u32 ticks;
    u32 told, tnow, tcnt = 0;
    u32 reload = SysTick->LOAD;
    ticks = nus * fac_us;
    told = SysTick->VAL;
    while(1)
    {
        tnow = SysTick->VAL;
        if(tnow != told)
        {
            if(tnow < told) tcnt += told - tnow; /* SYSTICK 是一个递减的计数器 */
            else tcnt += reload - tnow + told;
            told = tnow;
            if(tcnt >= ticks) break; /* 时间达到预期值，退出循环 */
        }
    }
}

/**
 * @brief  毫秒级延时 (自适应调度延时)
 * @param  nms 需要延时的毫秒数
 * @note   调度器启动后优先使用 vTaskDelay，不足一个 Tick 的时间通过硬件延时补足。
 */
void delay_ms(u32 nms)
{
    /* 判断 FreeRTOS 调度器是否已经启动 */
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) 
    {
        /* 调度器已启动：如果延时大于等于 1 个系统 Tick */
        if(nms >= (1000 / configTICK_RATE_HZ))
        {
            /* 使用 FreeRTOS 的 API 释放 CPU 使用权 */
            vTaskDelay(nms / (1000 / configTICK_RATE_HZ));
        }
        /* 剩余不足一个 Tick 的时间量 */
        nms %= (1000 / configTICK_RATE_HZ); 
    }
    
    /* 如果调度器未启动，或存在不足一个 Tick 的余量，使用硬件延时 */
    if(nms) 
    {
        delay_us(1000 * nms);
    }
}

/**
 * @brief  获取系统运行的毫秒总数
 * @return u32 系统运行时间 (ms)
 * @note   主要用于兼容时序逻辑，基于 FreeRTOS 的 Tick 计数。
 */
u32 millis(void)
{
    /* 检查 FreeRTOS 调度器是否已经启动 */
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        return xTaskGetTickCount(); 
    }
    /* 如果系统尚未启动，默认返回 0 */
    return 0; 
}
