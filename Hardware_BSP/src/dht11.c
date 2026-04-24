/**
 * @file    dht11.c
 * @brief   DHT11 温湿度传感器单总线协议实现
 */
#include "dht11.h"
#include "delay.h"

/**
 * @brief  配置为输出模式
 */
static void DHT11_IO_OUT(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(DHT11_PORT, &GPIO_InitStructure);
}

/**
 * @brief  配置为输入模式
 */
static void DHT11_IO_IN(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(DHT11_PORT, &GPIO_InitStructure);
}

#define DHT11_DQ_OUT(x) do{ if(x) GPIO_SetBits(DHT11_PORT,DHT11_PIN); else GPIO_ResetBits(DHT11_PORT,DHT11_PIN); }while(0)
#define DHT11_DQ_IN  GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN)

/**
 * @brief  主设备发送复位/起始信号
 */
static void DHT11_Rst(void) {                 
    DHT11_IO_OUT(); 
    DHT11_DQ_OUT(0); 
    delay_ms(20);    // 拉低至少 18ms
    DHT11_DQ_OUT(1); 
    delay_us(30);    // 拉高 20~40us
}

/**
 * @brief  检测 DHT11 的响应信号
 */
static u8 DHT11_Check(void) {   
    u8 retry = 0;
    DHT11_IO_IN(); // 设为输入
    while (DHT11_DQ_IN && retry < 100) { retry++; delay_us(1); } 
    if(retry >= 100) return 1; else retry = 0;
    while (!DHT11_DQ_IN && retry < 100) { retry++; delay_us(1); }
    if(retry >= 100) return 1;
    return 0;
}

/**
 * @brief  读取总线单比特数据
 */
static u8 DHT11_Read_Bit(void) {
    u8 retry = 0;
    while(DHT11_DQ_IN && retry < 100) { retry++; delay_us(1); }
    retry = 0;
    while(!DHT11_DQ_IN && retry < 100) { retry++; delay_us(1); }
    delay_us(40); // 等待 40us 判定电平长短
    if(DHT11_DQ_IN) return 1; else return 0;
}

/**
 * @brief  读取总线单字节数据
 */
static u8 DHT11_Read_Byte(void) {        
    u8 i, dat;
    dat = 0;
    for (i = 0; i < 8; i++) {
        dat <<= 1; 
        dat |= DHT11_Read_Bit();
    }                           
    return dat;
}

/**
 * @brief  读取完整温湿度数据包
 * @param  temp 温度结果输出指针
 * @param  humi 湿度结果输出指针
 * @return u8   状态 (0:成功; 1:失败)
 */
u8 DHT11_Read_Data(u8 *temp, u8 *humi) {        
    u8 buf[5], i;
    DHT11_Rst();
    if(DHT11_Check() == 0) {
        for(i = 0; i < 5; i++) { buf[i] = DHT11_Read_Byte(); }
        // 校验和机制
        if((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4]) {
            *humi = buf[0];
            *temp = buf[2];
        }
    } else return 1;
    return 0;      
}

/**
 * @brief  传感器引脚初始化
 */
u8 DHT11_Init(void) {
    RCC_AHB1PeriphClockCmd(DHT11_RCC, ENABLE);
    DHT11_Rst();
    return DHT11_Check();
}
