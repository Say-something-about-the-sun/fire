// usart2.c - USART2ʵ���ļ�

#include "usart2.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

// ESP32数据接收缓冲区定义
ESP32_SensorData g_esp32_data;

u8 ESP32_RX_BUF[ESP32_RX_BUF_SIZE];
u16 ESP32_RX_STA = 0;

// USART2��ʼ��
void USART2_Init(u32 baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    
    // 1. ʹ��ʱ��
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    
    // 2. ����GPIO
    GPIO_PinAFConfig(USART2_TX_PORT, GPIO_PinSource2, GPIO_AF_USART2); // PA2 -> USART2_TX
    GPIO_PinAFConfig(USART2_RX_PORT, GPIO_PinSource3, GPIO_AF_USART2); // PA3 -> USART2_RX
    
    GPIO_InitStructure.GPIO_Pin = USART2_TX_PIN | USART2_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 3. ����USART
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStructure);
    
    // 4. ʹ��USART2
    USART_Cmd(USART2, ENABLE);
    USART_ClearFlag(USART2, USART_FLAG_TC);
}

// USART2�ж�����
void USART2_ConfigInterrupt(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    
    // ʹ��USART2�����ж�
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    
    // ����NVIC
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

// USART2����һ���ֽ�
void USART2_Send_Byte(u8 data)
{
    // �ȴ����ͻ�����Ϊ��
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    
    // ��������
    USART_SendData(USART2, data);
    
    // �ȴ��������
    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
}

// USART2发送字符串
void USART2_Send_String(char* str)
{
    if(str == 0) return;
    
    while(*str)
    {
        USART2_Send_Byte((u8)(*str));
        str++;
    }
}

// USART2中断处理函数
void USART2_IRQHandler(void)
{
    u8 Res;
    
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        Res = USART_ReceiveData(USART2);
        
        if((ESP32_RX_STA & 0x8000) == 0) // 接收未完成
        {
            if(Res == 0x0a) // 接收到0x0a，接收完成（支持单独的\n作为结束符）
            {
                ESP32_RX_STA |= 0x8000;
            }
            else if(ESP32_RX_STA & 0x4000) // 接收到0x0d
            {
                if(Res == 0x0a) // 接收到0x0a，接收完成（支持\r\n作为结束符）
                {
                    ESP32_RX_STA |= 0x8000;
                }
                else // 接收错误，重新开始
                {
                    ESP32_RX_STA = 0;
                }
            }
            else // 未接收到0x0d
            {
                if(Res == 0x0d)
                {
                    ESP32_RX_STA |= 0x4000;
                }
                else
                {
                    ESP32_RX_BUF[ESP32_RX_STA & 0x3FFF] = Res;
                    ESP32_RX_STA++;
                    if(ESP32_RX_STA > (ESP32_RX_BUF_SIZE - 1))
                    {
                        ESP32_RX_STA = 0; // 接收缓冲区满，重新开始
                    }
                }
            }
        }
    }
		
		if(USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) // 注意：在usart2.c改成USART2，usart3.c改成USART3
    {
        USART_ReceiveData(USART2); // 读取一次数据即可清除硬件 ORE 标志位，解救单片机
    }
		
		
}


void USART2_Process_ESP32_Data(void)
{
    // 检查是否接收到完整的一帧数据 (以 \r\n 结尾)
    if(ESP32_RX_STA & 0x8000)
    {
        // 1. 获取数据长度，并添加字符串结束符
        u16 len = ESP32_RX_STA & 0x3FFF;
        ESP32_RX_BUF[len] = '\0'; 
        
        char* str = (char*)ESP32_RX_BUF;
        
        // 2. 判断是否是【时间数据】： TIME:YYYY-MM-DD HH:MM:SS
        if(strncmp(str, "TIME:", 5) == 0)
        {
            // 提取时间字符串，跳过 "TIME:" 这5个字符
            strncpy(g_esp32_data.ntp_time, str + 5, sizeof(g_esp32_data.ntp_time) - 1);
            g_esp32_data.ntp_time[sizeof(g_esp32_data.ntp_time) - 1] = '\0';
            g_esp32_data.time_updated = 1; // 标记时间已更新
            
            // 给 ESP32 回复 OK (虽然只有发送数据时才强制等OK，但回一个更稳妥)
            USART2_Send_String("OK\r\n");
        }
        // 3. 判断是否是【传感器数据】： FLAME_AO:xxx,FLAME_DO:x,FIRE:x
        else if(strncmp(str, "FLAME_AO:", 9) == 0)
        {
            int ao = 0, do_val = 0, fire = 0;
            
            // 使用 sscanf 提取对应格式的数字
            if(sscanf(str, "FLAME_AO:%d,FLAME_DO:%d,FIRE:%d", &ao, &do_val, &fire) == 3)
            {
                g_esp32_data.flame_adc = (u16)ao;
                g_esp32_data.flame_do = (u8)do_val;
                g_esp32_data.fire_detected = (u8)fire;
            }
            
            // 🚨 关键：必须给 ESP32 回复 OK，否则 ESP32 的 waitForSTM32Response() 会苦等2秒！
            USART2_Send_String("OK\r\n");
        }
        
        // 4. 清空接收缓冲区，准备接收下一轮数据
        ESP32_RX_STA = 0;
        memset(ESP32_RX_BUF, 0, ESP32_RX_BUF_SIZE);
    }
}

