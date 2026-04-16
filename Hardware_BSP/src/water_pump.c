#include "water_pump.h"

// 记录水泵当前状态：1=开，0=关
static u8 pump_status = 0;

/**
 * @brief  初始化水泵 N-MOSFET 控制引脚
 */
void WaterPump_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    // 1. 使能 GPIO 时钟
    RCC_AHB1PeriphClockCmd(PUMP_RCC, ENABLE);

    // 2. 配置引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin = PUMP_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    // 🚨 极其重要：配置下拉电阻！防止单片机刚上电复位时，引脚悬空导致误触发水泵喷水
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN; 
    GPIO_Init(PUMP_PORT, &GPIO_InitStructure);

    // 3. 初始状态：强制拉低关闭水泵
    PUMP_OFF();
    pump_status = 0;
}

void WaterPump_On(void) {
    GPIO_SetBits(PUMP_PORT, PUMP_PIN);
    pump_status = 1;
}

void WaterPump_Off(void) {
    GPIO_ResetBits(PUMP_PORT, PUMP_PIN);
    pump_status = 0;
}

// 给 JSON 打包获取状态用的
u8 WaterPump_GetStatus(void) {
    return pump_status;
}
