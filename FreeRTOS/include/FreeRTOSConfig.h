#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "sys.h"

/* -----------------------------------------------------------
 * Application specific definitions.
 * ---------------------------------------------------------- */

#define configUSE_PREEMPTION                    1           // 1: 抢占式调度, 0: 协程式调度
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1           // 启用硬件计算前导零(STM32支持)
#define configUSE_TICKLESS_IDLE                 0           // 1: 启用低功耗tickless模式
#define configCPU_CLOCK_HZ                      ( 168000000 ) // STM32F4 主频 168MHz
#define configTICK_RATE_HZ                      ( 1000 )    // 系统时钟节拍: 1000Hz (1ms)
#define configMAX_PRIORITIES                    ( 32 )      // 最大可用优先级数
#define configMINIMAL_STACK_SIZE                ( ( uint16_t ) 128 ) // 空闲任务最小栈大小(字=4字节)
#define configMAX_TASK_NAME_LEN                 ( 16 )      // 任务名最大长度
#define configUSE_16_BIT_TICKS                  0           // 0: 32位系统

/* 内存分配相关 (极其重要) */
#define configSUPPORT_DYNAMIC_ALLOCATION        1           // 支持动态内存分配
#define configSUPPORT_STATIC_ALLOCATION         0           // 不支持静态内存分配
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 20 * 1024 ) ) // FreeRTOS堆空间大小: 40KB (留给任务栈和信号量)
#define configAPPLICATION_ALLOCATED_HEAP        0           // 0: 编译器自动分配堆

/* 钩子函数配置 */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          0           // 建议调试时改为 1 或 2，发布改为 0
#define configUSE_MALLOC_FAILED_HOOK            0

/* 运行时间和任务状态统计相关 */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/* 软件定时器相关配置 */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

/* 可选功能配置 */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1

/* -----------------------------------------------------------
 * 中断嵌套行为配置 (STM32F4 必看)
 * ---------------------------------------------------------- */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS 4  // STM32 使用 4 位优先级 (0~15)
#endif

/* 这个优先级是系统可以屏蔽的最低优先级。STM32中 15 是最低的 */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15

/* 这个优先级是 FreeRTOS API 可以被安全调用的最高中断优先级。
   优先级高于 5 (即 0~4) 的中断，绝对不能调用任何带有 "FromISR" 后缀的 FreeRTOS API，
   否则系统立刻崩溃死机！ */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* -----------------------------------------------------------
 * FreeRTOS 核心中断映射 (替代 stm32f4xx_it.c 中的函数)
 * ---------------------------------------------------------- */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
