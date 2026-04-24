/**
 * @file    usart.c
 * @brief   USART1 底层驱动实现
 */
#include "sys.h"
#include "usart.h"  

#if 1
#pragma import(__use_no_semihosting)             
/* 标准库支持函数 */                 
struct __FILE 
{ 
    int handle; 
}; 

FILE __stdout;       
/* 定义_sys_exit() 以避免使用半主机模式 */    
void _sys_exit(int x) 
{ 
    x = x; 
} 
/* 重定义 fputc 函数支持 printf */ 
int fputc(int ch, FILE *f)
{   
    while((USART1->SR & 0X40) == 0); /* 阻塞等待发送缓冲区为空 */   
    USART1->DR = (u8) ch;      
    return ch;
}
#endif
 
#if EN_USART1_RX   
u8 USART_RX_BUF[USART_REC_LEN];     
u16 USART_RX_STA = 0;       

/**
 * @brief  初始化 USART1 接口
 * @param  bound 波特率配置值
 */
void uart_init(u32 bound){
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE); 
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
 
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1); 
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1); 
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; 
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; 
    GPIO_Init(GPIOA, &GPIO_InitStructure); 

    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; 
    USART_Init(USART1, &USART_InitStructure); 
    
    USART_Cmd(USART1, ENABLE);  
    USART_ClearFlag(USART1, USART_FLAG_TC);
    
#if EN_USART1_RX    
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;      
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;         
    NVIC_Init(&NVIC_InitStructure); 
#endif
}

/**
 * @brief  轮询发送单个字节
 */
void uart_send_byte(uint8_t byte)
{
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, byte);
    while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}

/**
 * @brief  USART1 硬件中断处理函数
 */
void USART1_IRQHandler(void)                    
{
    u8 Res;
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) 
    {
        Res = USART_ReceiveData(USART1);
        
        if((USART_RX_STA & 0x8000) == 0)
        {
            if(USART_RX_STA & 0x4000)
            {
                if(Res != 0x0a) USART_RX_STA = 0;
                else USART_RX_STA |= 0x8000;    
            }
            else 
            {   
                if(Res == 0x0d) USART_RX_STA |= 0x4000;
                else
                {
                    USART_RX_BUF[USART_RX_STA & 0X3FFF] = Res ;
                    USART_RX_STA++;
                    if(USART_RX_STA > (USART_REC_LEN - 1)) USART_RX_STA = 0;
                }        
            }
        }            
    } 
    
    /* 清除硬件溢出错误标志位，恢复系统接收状态 */
    if(USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) 
    {
        USART_ReceiveData(USART1); 
    }
} 
#endif  

/* USART1 DMA 发送配置 (挂载于 DMA2 Stream7 Channel4) */
volatile uint8_t usart1_dma_busy = 0;      
volatile uint8_t usart1_dma_complete = 0;  

/**
 * @brief  初始化 USART1 的 DMA 发送通道
 */
void usart1_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
    
    DMA_DeInit(DMA2_Stream7);
    while(DMA_GetCmdStatus(DMA2_Stream7) != DISABLE);
    
    DMA_InitStructure.DMA_Channel = DMA_Channel_4;  
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr = 0;  
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStructure.DMA_BufferSize = 0;  
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream7, &DMA_InitStructure);
    
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream7_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    
    usart1_dma_busy = 0;
}

/**
 * @brief  DMA2 Stream7 数据传输完成中断处理
 */
void DMA2_Stream7_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA2_Stream7, DMA_IT_TCIF7))
    {
        DMA_ClearITPendingBit(DMA2_Stream7, DMA_IT_TCIF7);
        DMA_Cmd(DMA2_Stream7, DISABLE);
        usart1_dma_busy = 0;
        usart1_dma_complete = 1;
    }
}

/**
 * @brief  触发一次 DMA 数据块发送
 * @param  data 发送缓冲区起始地址
 * @param  len  发送数据长度
 * @note   函数采用非阻塞设计，配置寄存器后立即返回。
 */
void usart1_dma_send(uint8_t* data, uint16_t len)
{
    if(len == 0) return;
    
    while(usart1_dma_busy);
    usart1_dma_busy = 1;
    
    DMA_Cmd(DMA2_Stream7, DISABLE);
    while(DMA_GetCmdStatus(DMA2_Stream7) != DISABLE);
    
    DMA2_Stream7->M0AR = (uint32_t)data;
    DMA2_Stream7->NDTR = len;
    
    DMA_ClearFlag(DMA2_Stream7, DMA_FLAG_TCIF7 | DMA_FLAG_HTIF7 | DMA_FLAG_TEIF7 | DMA_FLAG_DMEIF7 | DMA_FLAG_FEIF7);
    DMA_Cmd(DMA2_Stream7, ENABLE);
}
