#include "usart3.h"
#include "delay.h"

// 接收缓冲区
u8 USART3_RX_BUF[USART3_RX_BUF_SIZE];
u16 USART3_RX_STA = 0; // 接收状态标记

// 初始化USART3
void USART3_Init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 使能GPIO时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    // 使能USART时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    // 配置GPIO
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 连接GPIO到USART3
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3); // PB10 -> USART3_TX
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_USART3); // PB11 -> USART3_RX

    // 配置USART3
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    // 使能USART3
    USART_Cmd(USART3, ENABLE);

    printf("[USART3] 初始化完成，波特率: %lu\r\n", bound);
    printf("[USART3] 连接: STM32 PB10 (TX) -> ESP8266 RX\r\n");
    printf("[USART3] 连接: STM32 PB11 (RX) -> ESP8266 TX\r\n");
}

// 配置USART3中断
void USART3_ConfigInterrupt(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    // 使能USART3接收中断
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    // 配置NVIC
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    printf("[USART3] 中断配置完成\r\n");
}

// 发送字符串
void USART3_Send_String(char* str)
{
    while(*str)
    {
        while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
        USART_SendData(USART3, *str++);
    }
    while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}

// 发送数据
void USART3_Send_Data(u8* data, u16 len)
{
    u16 i;
    for(i = 0; i < len; i++)
    {
        while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
        USART_SendData(USART3, data[i]);
    }
    while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}

// USART3中断处理函数（纯净透传模式，接收所有字符）
// 纯净版 USART3 中断服务函数（专为 ESP8266 AT 指令优化）
// ===================================================
// 完美适配云端指令解析的 USART3 中断服务函数
// 能够精准识别 \r\n 结尾的数据帧，并置位 0x8000
// ===================================================
void USART3_IRQHandler(void)
{
    u8 Res;
    
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) // 接收到数据
    {
        Res = USART_ReceiveData(USART3); // 读取接收到的1个字节数据
        
        if((USART3_RX_STA & 0x8000) == 0) // 如果之前的一帧还没被 task 处理完，就不收新数据
        {
            if(USART3_RX_STA & 0x4000) // 已经接收到了回车符 0x0d (\r)
            {
                if(Res != 0x0a) {
                    USART3_RX_STA = 0; // 接收错误,不是 \n，重新开始
                } else {
                    USART3_RX_STA |= 0x8000; // 完美接收到 \n，最高位置 1，触发大脑解析！
                }
            }
            else // 还没收到 \r
            {
                if(Res == 0x0d) {
                    USART3_RX_STA |= 0x4000; // 收到 \r，标记一下
                } else {
                    // 把数据存入缓冲区
                    USART3_RX_BUF[USART3_RX_STA & 0X3FFF] = Res;
                    USART3_RX_STA++;
                    // 防止缓冲区溢出
                    if(USART3_RX_STA > (USART3_RX_BUF_SIZE - 1)) {
                        USART3_RX_STA = 0; // 接收错误，重新开始
                    }
                }
            }
        }
    }
    
    // 清除硬件 ORE 溢出错误标志位，防止单片机死机
    if(USART_GetFlagStatus(USART3, USART_FLAG_ORE) != RESET) 
    {
        USART_ReceiveData(USART3); 
    }
}
