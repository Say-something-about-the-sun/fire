#include "delay.h"
#include "sys.h"
// 引入 FreeRTOS 头文件
#include "FreeRTOS.h"
#include "task.h"

static u32 fac_us = 0; // us延时倍乘数

// 初始化延迟函数
// SYSCLK: 系统时钟频率 (STM32F4通常是 168)
// 初始化延迟函数
// SYSCLK: 系统时钟频率 (STM32F4通常是 168)
void delay_init(u8 SYSCLK)
{
    // 配置 SysTick 时钟源为 HCLK
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK); 
    fac_us = SYSCLK; 
    
    // 🚨 【防卡死补丁】：在 FreeRTOS 接管前，必须强行让滴答定时器“跳动”起来！
    // 否则所有在 main() 前期执行的 delay_us/ms 都会陷入死循环！
    SysTick->LOAD = 0xFFFFFF; // 填入最大重装载值
    SysTick->VAL = 0;         // 清空当前计数值
    SysTick->CTRL |= 5;       // 开启定时器 (BIT0=1) 并选择 HCLK (BIT2=1)，禁止中断 (BIT1=0)
}

// 微秒级延时 (纯硬件死等，不发生任务调度)
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
            if(tnow < told) tcnt += told - tnow; // 这里注意SYSTICK是一个递减的计数器
            else tcnt += reload - tnow + told;
            told = tnow;
            if(tcnt >= ticks) break; // 时间超过/等于要延迟的时间,则退出
        }
    }
}

// 毫秒级延时 (智能切换：裸机死等 / OS挂起)
void delay_ms(u32 nms)
{
    // 判断 FreeRTOS 调度器是否已经启动
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) 
    {
        // 调度器已启动：如果延时大于 1 个系统 Tick (通常是 1ms)
        if(nms >= (1000 / configTICK_RATE_HZ))
        {
            // 使用 FreeRTOS 的 vTaskDelay 释放 CPU 给其他任务
            vTaskDelay(nms / (1000 / configTICK_RATE_HZ));
        }
        // 剩下的不足一个 Tick 的时间，用微秒级硬件延时补足
        nms %= (1000 / configTICK_RATE_HZ); 
    }
    
    // 如果调度器未启动，或者还有零头时间，使用硬件延时
    if(nms) 
    {
        delay_us(1000 * nms);
    }
}

// 获取系统运行的毫秒数 (兼容老代码)
u32 millis(void)
{
    // 检查 FreeRTOS 调度器是否已经启动
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        // FreeRTOS 的 Tick 默认就是 1ms 增加一次，直接返回即可
        return xTaskGetTickCount(); 
    }
    // 如果系统还没启动，暂时返回 0
    return 0; 
}

