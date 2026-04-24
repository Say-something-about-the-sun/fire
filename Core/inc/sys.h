/**
 * @file    sys.h
 * @brief   系统级宏定义与汇编指令封装
 * @note    提供 STM32F4 的位带操作宏及部分底层的控制接口。
 */
#ifndef __SYS_H
#define __SYS_H     
#include "stm32f4xx.h" 

#define SYSTEM_SUPPORT_UCOS     0       /* 标识系统是否支持 UCOS (当前工程采用 FreeRTOS) */
                                        
/* 位带操作宏定义 (参考 CM3 权威指南，实现单比特引脚操作) */
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 

/* IO口地址映射 */
#define GPIOA_ODR_Addr    (GPIOA_BASE+20) 
#define GPIOB_ODR_Addr    (GPIOB_BASE+20) 
#define GPIOC_ODR_Addr    (GPIOC_BASE+20) 
#define GPIOD_ODR_Addr    (GPIOD_BASE+20)  
#define GPIOE_ODR_Addr    (GPIOE_BASE+20)  
#define GPIOF_ODR_Addr    (GPIOF_BASE+20)     
#define GPIOG_ODR_Addr    (GPIOG_BASE+20)    
#define GPIOH_ODR_Addr    (GPIOH_BASE+20)     
#define GPIOI_ODR_Addr    (GPIOI_BASE+20)      

#define GPIOA_IDR_Addr    (GPIOA_BASE+16) 
#define GPIOB_IDR_Addr    (GPIOB_BASE+16)  
#define GPIOC_IDR_Addr    (GPIOC_BASE+16)  
#define GPIOD_IDR_Addr    (GPIOD_BASE+16)  
#define GPIOE_IDR_Addr    (GPIOE_BASE+16)  
#define GPIOF_IDR_Addr    (GPIOF_BASE+16)  
#define GPIOG_IDR_Addr    (GPIOG_BASE+16)  
#define GPIOH_IDR_Addr    (GPIOH_BASE+16)  
#define GPIOI_IDR_Addr    (GPIOI_BASE+16)  
 
/* IO口读写操作宏 */
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)   
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)   
#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)   
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)   
#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)   
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)   
#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)   
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)   
#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)   
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)  
#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)   
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)  
#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)   
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)  
#define PHout(n)   BIT_ADDR(GPIOH_ODR_Addr,n)   
#define PHin(n)    BIT_ADDR(GPIOH_IDR_Addr,n)  
#define PIout(n)   BIT_ADDR(GPIOI_ODR_Addr,n)   
#define PIin(n)    BIT_ADDR(GPIOI_IDR_Addr,n)  

/* 汇编函数接口声明 */
void WFI_SET(void);       /* 执行 WFI 休眠指令 */
void INTX_DISABLE(void);  /* 关闭全局中断 */
void INTX_ENABLE(void);   /* 开启全局中断 */
void MSR_MSP(u32 addr);   /* 设置主堆栈地址 */ 

#endif /* __SYS_H */
