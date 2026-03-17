#include "flame.h"

// ADC初始化（内部函数，直接配置PA0）
static void Flame_ADC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_InitTypeDef ADC_InitStruct;
    
    // 1. 使能GPIOA和ADC1时钟（直接写死）
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    
    // 2. 配置PA0为模拟输入（直接指定PA0）
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;  // 模拟输入模式
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL; // 无上下拉
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 3. 配置ADC1（固定参数）
    ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b; // 12位精度
    ADC_InitStruct.ADC_ScanConvMode = DISABLE; // 单通道模式
    ADC_InitStruct.ADC_ContinuousConvMode = ENABLE; // 连续转换
    ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None; // 软件触发
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right; // 右对齐
    ADC_InitStruct.ADC_NbrOfConversion = 1; // 转换通道数
    ADC_Init(ADC1, &ADC_InitStruct);
    
    // 4. 配置ADC1通道0（对应PA0）
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_480Cycles);
    
    // 5. 使能ADC并校准
    ADC_Cmd(ADC1, ENABLE);
    
}

// DO引脚初始化（内部函数，直接配置PB0）
static void Flame_DO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    
    // 1. 使能GPIOB时钟（直接写死）
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    
    // 2. 配置PB0为上拉输入（直接指定PB0）
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN; // 输入模式
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP; // 上拉（防止悬空）
    GPIO_Init(GPIOB, &GPIO_InitStruct);
}

// 火焰传感器整体初始化
void Flame_Sensor_Init(void)
{
    Flame_ADC_Init();
    Flame_DO_Init();
}

// 读取PA0的ADC值
uint16_t Flame_Get_ADC_Value(void)
{
    ADC_SoftwareStartConv(ADC1); // 软件触发ADC1转换
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC)); // 等待转换完成
    return ADC_GetConversionValue(ADC1); // 返回12位ADC值（0-4095）
}

// 读取PB0的数字状态
uint8_t Flame_Get_DO_State(void)
{
    return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0);
}
