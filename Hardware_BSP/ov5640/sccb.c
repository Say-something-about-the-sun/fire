/**
 * @file    sccb.c
 * @brief   SCCB 协议底层实现
 * @note    基于 GPIO 模拟 SCCB 时序，参考标准摄像头硬件驱动规范。
 */
#include "sys.h"
#include "sccb.h"
#include "delay.h"

/**
 * @brief  初始化 SCCB 控制引脚
 */
void SCCB_Init(void)
{               
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;  /* 采用推挽输出模式 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
 
    GPIO_SetBits(GPIOD, GPIO_Pin_6 | GPIO_Pin_7);
    SCCB_SDA_OUT();        
}

/**
 * @brief  产生 SCCB 起始信号
 */
void SCCB_Start(void)
{
    SCCB_SDA = 1;     
    SCCB_SCL = 1;    
    delay_us(2);  
    SCCB_SDA = 0;
    delay_us(2);     
    SCCB_SCL = 0;     
}

/**
 * @brief  产生 SCCB 停止信号
 */
void SCCB_Stop(void)
{
    SCCB_SDA = 0;
    delay_us(2);     
    SCCB_SCL = 1;    
    delay_us(2); 
    SCCB_SDA = 1;    
    delay_us(2);
}  

/**
 * @brief  产生非应答信号 (NAK)
 */
void SCCB_No_Ack(void)
{
    delay_us(2);
    SCCB_SDA = 1;    
    SCCB_SCL = 1;    
    delay_us(2);
    SCCB_SCL = 0;    
    delay_us(2);
    SCCB_SDA = 0;    
    delay_us(2);
}

/**
 * @brief  SCCB 写入一个字节
 * @param  dat 待写入的数据
 * @return u8  应答状态 (0:成功, 1:失败)
 */
u8 SCCB_WR_Byte(u8 dat)
{
    u8 j, res;     
    for(j = 0; j < 8; j++) 
    {
        if(dat & 0x80) SCCB_SDA = 1;    
        else           SCCB_SDA = 0;
        dat <<= 1;
        delay_us(2);
        SCCB_SCL = 1;    
        delay_us(2);
        SCCB_SCL = 0;           
    }          
    SCCB_SDA_IN();      
    delay_us(2);
    SCCB_SCL = 1;         
    delay_us(2);
    if(SCCB_READ_SDA) res = 1;  
    else              res = 0;         
    SCCB_SCL = 0;        
    SCCB_SDA_OUT();     
    return res;  
}     

/**
 * @brief  SCCB 读取一个字节
 * @return u8 读取到的数据
 */
u8 SCCB_RD_Byte(void)
{
    u8 temp = 0, j;    
    SCCB_SDA_IN();      
    for(j = 8; j > 0; j--)    
    {                     
        delay_us(2);
        SCCB_SCL = 1;
        temp = temp << 1;
        if(SCCB_READ_SDA) temp++;    
        delay_us(2);
        SCCB_SCL = 0;
    }    
    SCCB_SDA_OUT();     
    return temp;
}

/**
 * @brief  写寄存器 (8位地址设备兼容模式)
 * @param  reg  寄存器地址
 * @param  data 待写入数据
 * @return u8   操作状态 (0:成功, 1:失败)
 */
u8 SCCB_WR_Reg(u8 reg, u8 data)
{
    u8 res = 0;
    SCCB_Start();                   
    if(SCCB_WR_Byte(SCCB_ID)) res = 1;  
    delay_us(100);
    if(SCCB_WR_Byte(reg))     res = 1;     
    delay_us(100);
    if(SCCB_WR_Byte(data))    res = 1;     
    SCCB_Stop();      
    return res;
}           

/**
 * @brief  读寄存器 (8位地址设备兼容模式)
 * @param  reg 寄存器地址
 * @return u8  读取到的数据
 */
u8 SCCB_RD_Reg(u8 reg)
{
    u8 val = 0;
    SCCB_Start();               
    SCCB_WR_Byte(SCCB_ID);      
    delay_us(100);     
    SCCB_WR_Byte(reg);          
    delay_us(100);      
    SCCB_Stop();   
    delay_us(100);       
    SCCB_Start();
    SCCB_WR_Byte(SCCB_ID | 0X01);  
    delay_us(100);
    val = SCCB_RD_Byte();         
    SCCB_No_Ack();
    SCCB_Stop();
    return val;
}

/**
 * @brief  OV5640 硬件复位时序
 * @note   控制 PWDN 与 RST 引脚，确保摄像头芯片正常启动
 */
void OV5640_HW_Reset(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);
    
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_15;  /* PG9=PWDN, PG15=RST */
    GPIO_Init(GPIOG, &GPIO_InitStructure);
    
    OV5640_RST_L();   /* 拉低复位信号 */
    delay_ms(20); 
    
    OV5640_PWDN_L();  /* 退出掉电模式 */
    delay_ms(5);  
    
    OV5640_RST_H();   /* 释放复位信号，进入正常工作状态 */
    delay_ms(20);
}
