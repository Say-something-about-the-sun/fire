/**
 * @file    sccb.h
 * @brief   SCCB (Serial Camera Control Bus) 底层总线驱动接口
 * @note    用于与 OV5640 摄像头模块进行寄存器级别的配置通信。
 */
#ifndef __SCCB_H
#define __SCCB_H
#include "sys.h"

/* IO操作定义 (SDA引脚方向控制) */
#define SCCB_SDA_IN()  {GPIOD->MODER&=~(3<<(7*2));GPIOD->MODER|=0<<7*2;} /* PD7 输入 */
#define SCCB_SDA_OUT() {GPIOD->MODER&=~(3<<(7*2));GPIOD->MODER|=1<<7*2;} /* PD7 输出 */

/* IO电平控制宏 */     
#define SCCB_SCL        PDout(6)    /* SCL 时钟线 */
#define SCCB_SDA        PDout(7)    /* SDA 数据线 */     
#define SCCB_READ_SDA   PDin(7)     /* 读 SDA 状态 */ 

/* OV5640 硬件控制引脚 */
#define OV5640_PWDN_PIN  GPIO_Pin_9
#define OV5640_PWDN_PORT GPIOG
#define OV5640_PWDN_H()  GPIO_SetBits(OV5640_PWDN_PORT, OV5640_PWDN_PIN)
#define OV5640_PWDN_L()  GPIO_ResetBits(OV5640_PWDN_PORT, OV5640_PWDN_PIN)

#define OV5640_RST_PIN   GPIO_Pin_15
#define OV5640_RST_PORT  GPIOG
#define OV5640_RST_H()   GPIO_SetBits(OV5640_RST_PORT, OV5640_RST_PIN)
#define OV5640_RST_L()   GPIO_ResetBits(OV5640_RST_PORT, OV5640_RST_PIN)

/* 摄像头设备地址定义 */
#define SCCB_ID         0x78        
#define OV5640_ADDR     0x78
#define OV5640_WR_ADDR  0x78  
#define OV5640_RD_ADDR  0x79  

/* 函数声明 */
void SCCB_Init(void);
void SCCB_Start(void);
void SCCB_Stop(void);
void SCCB_No_Ack(void);
u8   SCCB_WR_Byte(u8 dat);
u8   SCCB_RD_Byte(void); 
u8   SCCB_WR_Reg(u8 reg, u8 data);
u8   SCCB_RD_Reg(u8 reg);

void OV5640_HW_Reset(void);

#endif /* __SCCB_H */
