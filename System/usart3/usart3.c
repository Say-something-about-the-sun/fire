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
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
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
void USART3_IRQHandler(void)
{
    u8 res;
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) // 接收到数据
    {
        res = USART_ReceiveData(USART3); // 读取数据
        
        // 只要缓冲区没满，就一直存入数据，不做任何 \r\n 判断
        if(USART3_RX_STA < USART3_RX_BUF_SIZE - 1)
        {
            USART3_RX_BUF[USART3_RX_STA] = res;
            USART3_RX_STA++;
            // 实时保证字符串以 \0 结尾，方便后续使用 strstr 函数匹配
            USART3_RX_BUF[USART3_RX_STA] = '\0'; 
        }
    }
		
		if(USART_GetFlagStatus(USART3, USART_FLAG_ORE) != RESET) // 注意：在usart2.c改成USART2，usart3.c改成USART3
    {
        USART_ReceiveData(USART3); // 读取一次数据即可清除硬件 ORE 标志位，解救单片机
    }
		
}
