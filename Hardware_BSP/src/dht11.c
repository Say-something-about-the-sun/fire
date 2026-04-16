#include "dht11.h"
#include "delay.h"

// 快速切换 GPIO 方向的内部函数
static void DHT11_IO_OUT(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(DHT11_PORT, &GPIO_InitStructure);
}

static void DHT11_IO_IN(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(DHT11_PORT, &GPIO_InitStructure);
}

// 设置数据线高低电平
#define DHT11_DQ_OUT(x) do{ if(x) GPIO_SetBits(DHT11_PORT,DHT11_PIN); else GPIO_ResetBits(DHT11_PORT,DHT11_PIN); }while(0)
// 读取数据线电平
#define DHT11_DQ_IN  GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN)

// 复位 DHT11
static void DHT11_Rst(void) {                 
    DHT11_IO_OUT(); 
    DHT11_DQ_OUT(0); 
    delay_ms(20);    // 拉低至少 18ms
    DHT11_DQ_OUT(1); 
    delay_us(30);    // 拉高 20~40us
}

// 等待 DHT11 回应
static u8 DHT11_Check(void) {   
    u8 retry = 0;
    DHT11_IO_IN(); // 设为输入
    while (DHT11_DQ_IN && retry < 100) { retry++; delay_us(1); } 
    if(retry >= 100) return 1; else retry = 0;
    while (!DHT11_DQ_IN && retry < 100) { retry++; delay_us(1); }
    if(retry >= 100) return 1;
    return 0;
}

// 从 DHT11 读取一个位
static u8 DHT11_Read_Bit(void) {
    u8 retry = 0;
    while(DHT11_DQ_IN && retry < 100) { retry++; delay_us(1); }
    retry = 0;
    while(!DHT11_DQ_IN && retry < 100) { retry++; delay_us(1); }
    delay_us(40); // 等待 40us
    if(DHT11_DQ_IN) return 1; else return 0;
}

// 从 DHT11 读取一个字节
static u8 DHT11_Read_Byte(void) {        
    u8 i, dat;
    dat = 0;
    for (i = 0; i < 8; i++) {
        dat <<= 1; 
        dat |= DHT11_Read_Bit();
    }                           
    return dat;
}

// 核心：读取一次温湿度数据
u8 DHT11_Read_Data(u8 *temp, u8 *humi) {        
    u8 buf[5], i;
    DHT11_Rst();
    if(DHT11_Check() == 0) {
        for(i = 0; i < 5; i++) { buf[i] = DHT11_Read_Byte(); }
        if((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4]) {
            *humi = buf[0];
            *temp = buf[2];
        }
    } else return 1;
    return 0;      
}

u8 DHT11_Init(void) {
    RCC_AHB1PeriphClockCmd(DHT11_RCC, ENABLE);
    DHT11_Rst();
    return DHT11_Check();
}
