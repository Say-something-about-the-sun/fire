/**
 * @file    smoke.c
 * @brief   MQ-2 烟雾传感器底层驱动实现
 * @note    基于 ADC2 通道 5 (PA5) 采集模拟电压，并转换为大致的 PPM 浓度。
 */
#include "smoke.h"
#include <stdio.h>

/**
 * @brief  ADC 初始化 (内部调用)
 * @param  无
 * @retval 无
 * @note   配置 PA5 为模拟输入，开启 ADC2 单通道单次转换模式
 */
static void Smoke_ADC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_InitTypeDef  ADC_InitStruct;
    
    // 1. 使能 GPIOA 和 ADC2 时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);
    
    // 2. 配置 PA5 为模拟输入模式
    GPIO_InitStruct.GPIO_Pin  = GPIO_Pin_5;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;     // 模拟输入模式
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL; // 无上下拉
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 3. 配置 ADC2 常规参数
    ADC_InitStruct.ADC_Resolution           = ADC_Resolution_12b;             // 12位分辨率
    ADC_InitStruct.ADC_ScanConvMode         = DISABLE;                        // 单通道模式关闭扫描
    ADC_InitStruct.ADC_ContinuousConvMode   = DISABLE;                        // 关闭连续转换
    ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;  // 软件触发
    ADC_InitStruct.ADC_DataAlign            = ADC_DataAlign_Right;            // 右对齐
    ADC_InitStruct.ADC_NbrOfConversion      = 1;                              // 转换通道数为 1
    ADC_Init(ADC2, &ADC_InitStruct);
    
    // 4. 配置 ADC2 通道 5 (对应 PA5)，采样时间 480 个周期
    ADC_RegularChannelConfig(ADC2, ADC_Channel_5, 1, ADC_SampleTime_480Cycles);
    
    // 5. 使能 ADC
    ADC_Cmd(ADC2, ENABLE);
}

/**
 * @brief  烟雾传感器总体初始化
 */
void Smoke_Sensor_Init(void)
{
    Smoke_ADC_Init();
}

/**
 * @brief  获取一次 ADC 采样值
 * @param  无
 * @retval ADC 原始数值 (0~4095)，超时返回 0
 */
uint16_t Smoke_Get_ADC_Value(void)
{
    uint32_t timeout = 0;
    
    // 触发软件转换
    ADC_SoftwareStartConv(ADC2); 
    
    // 轮询等待转换结束 (带超时防死锁机制)
    while(!ADC_GetFlagStatus(ADC2, ADC_FLAG_EOC)) 
    {
        timeout++;
        if(timeout > 10000) 
        {
            printf("[Smoke] ADC Timeout Error!\r\n");
            return 0; 
        }
    }
    
    return ADC_GetConversionValue(ADC2); 
}

/**
 * @brief  获取烟雾大致浓度 (PPM)
 * @param  无
 * @retval 烟雾浓度浮点值
 * @note   基于分压定理与 MQ-2 阻值特性计算，已加入底噪过滤和除零保护。
 */
float Smoke_Get_Concentration(void)
{
    uint16_t adc_value = Smoke_Get_ADC_Value();
    float voltage = (float)adc_value * 3.3f / 4095.0f;
    
    // 保护机制：绝对禁止除以 0！如果电压过低视为无烟雾
    if (voltage <= 0.05f) 
    {
        return 0.0f;
    }
    
    // 根据传感器分压电路计算 Rs/Ro 比值
    float rs_ro_ratio = (3.3f - voltage) / voltage;
    float ppm = 0.0f;
    
    // 仅在有效量程内进行线性化换算
    if (rs_ro_ratio > 0.1f && rs_ro_ratio < 10.0f)
    {
        ppm = (rs_ro_ratio - 0.5f) * 100.0f; 
        if (ppm < 0) ppm = 0.0f;
    }
    
    return ppm;
}
