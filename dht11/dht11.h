#ifndef __DHT11_H
#define __DHT11_H 
#include "sys.h"

// DHT11 引脚定义 (可根据实际接线修改)
#define DHT11_PORT      GPIOB
#define DHT11_PIN       GPIO_Pin_14
#define DHT11_RCC       RCC_AHB1Periph_GPIOB

// 初始化与读取函数
u8 DHT11_Init(void);
u8 DHT11_Read_Data(u8 *temp, u8 *humi);

#endif
