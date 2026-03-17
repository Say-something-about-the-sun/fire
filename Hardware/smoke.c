#include "smoke.h"

// ADC初始化，内部函数，直接配置PA1
static void Smoke_ADC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_InitTypeDef ADC_InitStruct;
    
    // 1. 使能GPIOA和ADC2时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);
    
    // 2. 配置PA1为模拟输入（直接指定PA1）
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;  // 模拟输入模式
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL; // 无上下拉
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 3. 配置ADC2的常规配置
    ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b; // 12位分辨率
    ADC_InitStruct.ADC_ScanConvMode = DISABLE; // 单通道模式
    ADC_InitStruct.ADC_ContinuousConvMode = ENABLE; // 连续转换
    ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None; // 无外部触发
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right; // 右对齐
    ADC_InitStruct.ADC_NbrOfConversion = 1; // 转换通道数
    ADC_Init(ADC2, &ADC_InitStruct);
    
    // 4. 配置ADC2通道1（对应PA1）
    ADC_RegularChannelConfig(ADC2, ADC_Channel_1, 1, ADC_SampleTime_480Cycles);
    
    // 5. 使能ADC并校准
    ADC_Cmd(ADC2, ENABLE);
}

// 烟雾传感器总体初始化
void Smoke_Sensor_Init(void)
{
    Smoke_ADC_Init();
}

// 获取PA1的ADC值
uint16_t Smoke_Get_ADC_Value(void)
{
    ADC_SoftwareStartConv(ADC2); // 软件启动ADC2转换
    while(!ADC_GetFlagStatus(ADC2, ADC_FLAG_EOC)); // 等待转换完成
    return ADC_GetConversionValue(ADC2); // 返回12位ADC值（0-4095）
}

// 获取烟雾浓度（PPM）
// 注意：这是一个简化的转换公式，实际使用时需要根据MQ-2的数据手册进行校准
// MQ-2的电阻值随烟雾浓度变化，需要根据实际硬件电路（分压电路）进行校准
float Smoke_Get_Concentration(void)
{
    uint16_t adc_value = Smoke_Get_ADC_Value();
    float voltage = (float)adc_value * 3.3f / 4095.0f;
    
    // 【关键修复 1】：绝对禁止除以 0！如果电压太低，直接返回 0 浓度
    if (voltage <= 0.05f) 
    {
        return 0.0f;
    }
    
    float rs_ro_ratio = (3.3f - voltage) / voltage;
    float ppm = 0.0f;
    
    if (rs_ro_ratio > 0.1f && rs_ro_ratio < 10.0f)
    {
        ppm = (rs_ro_ratio - 0.5f) * 100.0f; 
        if (ppm < 0) ppm = 0;
    }
    
    return ppm;
}
