// dcmi.c - 直接使用正点原子的成功案例
#include "sys.h"
#include "dcmi.h" 
#include "delay.h"
#include "usart.h"
#include "jpeg_serial.h"
#include "led.h"
#include <stdio.h>
#include <string.h>

u8 ov_frame=0;   						//帧计数
extern void jpeg_data_process(void);	//JPEG数据处理函数
extern volatile u8 frame_done;		//帧完成标志

// 帧计数
volatile u32 g_raw_frame_count = 0;  // 硬件触发次数（中断进入次数）

// JPEG缓冲区大小定义
#define JPEG_MAX_SIZE       (32*1024)       // 每帧最大32KB

DCMI_InitTypeDef DCMI_InitStructure;

// 外部变量声明（在jpeg_serial.c中定义）
extern volatile BufferControl* current_capture_buf;
extern volatile BufferControl* completed_buffer;
extern volatile u8 dma_transfer_complete;
extern volatile BufferControl buf_ctrl_a;
extern volatile BufferControl buf_ctrl_b;
extern volatile u32 dropped_frames;

//DCMI中断服务函数 - 使用帧中断切换缓冲区（不关闭DCMI）
void DCMI_IRQHandler(void)
{
    // 验证中断是否触发，直接翻转LED1
    static u8 led1_state = 0;
    if(led1_state == 0) {
        led_on(led1);
        led1_state = 1;
    } else {
        led_off(led1);
        led1_state = 0;
    }
    
    if(DCMI_GetITStatus(DCMI_IT_FRAME) != RESET)
    {
        // 增加硬件帧计数
        g_raw_frame_count++;
        
        //1. 停止 DMA 並計算長度
        DMA_Cmd(DMA2_Stream1, DISABLE);
        u32 words_remaining = DMA_GetCurrDataCounter(DMA2_Stream1);
        current_capture_buf->data_len = (JPEG_MAX_SIZE - (words_remaining * 4));
        
        // 2. 標記當前緩衝區已完成
        current_capture_buf->state = BUF_READY;
        
        // 3. 乒乓切換
        volatile BufferControl* next_buf = (current_capture_buf == &buf_ctrl_a) ? &buf_ctrl_b : &buf_ctrl_a;
        if(next_buf->state == BUF_IDLE)
        {
            current_capture_buf = next_buf;
        }
        else
        {
            dropped_frames++;
        }
        
        // 4. 直接处理当前帧数据
        // 每5帧进行一次火焰检测
        static u8 fire_detect_cnt = 0;
        fire_detect_cnt++;
        process_jpeg_frame(fire_detect_cnt >= 5 ? 1 : 0);  // 1=进行火焰检测，0=只发送图像
        if(fire_detect_cnt >= 5) fire_detect_cnt = 0;
        
        // 5. 設置通知標誌並立即重啟 DMA，準備捕獲下一幀
        dma_transfer_complete = 1;
        
        // 配置DMA到新的缓冲区
        DMA2_Stream1->M0AR = (u32)current_capture_buf->buffer;
        DMA2_Stream1->NDTR = JPEG_MAX_SIZE / 4;
        
        // 清除DMA标志
        DMA_ClearFlag(DMA2_Stream1, DMA_FLAG_TCIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_FEIF1);
        
        // 强制清除所有DCMI标志位，确保状态机彻底复位
        DCMI->ICR = 0x1F; // 强制清除所有5个中断标志位（帧、行、溢出、同步错误等）
        
        // 先开启DMA，再保持DCMI捕获
        DMA_Cmd(DMA2_Stream1, ENABLE);
        // 不关闭DCMI，保持连续捕获模式，避免数据流断裂
        
        DCMI_ClearITPendingBit(DCMI_IT_FRAME);
    }
}
//DCMI DMA配置
//DMA_Memory0BaseAddr:存储器0地址    即要存储摄像头数据的内存地址(也可以是外设地址)
//DMA_BufferSize:存储器大小    0~65535
//DMA_MemoryDataSize:存储器位宽  
//DMA_MemoryDataSize:存储器位宽    @defgroup DMA_memory_data_size :DMA_MemoryDataSize_Byte/DMA_MemoryDataSize_HalfWord/DMA_MemoryDataSize_Word
//DMA_MemoryInc:存储器递增模式  @defgroup DMA_memory_incremented_mode  /** @defgroup DMA_memory_incremented_mode : DMA_MemoryInc_Enable/DMA_MemoryInc_Disable
void DCMI_DMA_Init(u32 DMA_Memory0BaseAddr,u16 DMA_BufferSize,u32 DMA_MemoryDataSize,u32 DMA_MemoryInc)
{ 
	DMA_InitTypeDef  DMA_InitStructure;
	
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2,ENABLE);//DMA2时钟使能 
	DMA_DeInit(DMA2_Stream1);
	while (DMA_GetCmdStatus(DMA2_Stream1) != DISABLE){}//等待DMA2_Stream1可配置 
	
  /* 配置 DMA Stream */
  DMA_InitStructure.DMA_Channel = DMA_Channel_1;  //通道1 DCMI通道 
  DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&DCMI->DR;//外设地址为:DCMI->DR
  DMA_InitStructure.DMA_Memory0BaseAddr = DMA_Memory0BaseAddr;//DMA 存储器0地址
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;//外设到存储器模式
  DMA_InitStructure.DMA_BufferSize = DMA_BufferSize;//数据传输量 
  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//外设非增量模式
  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;//存储器增量模式
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;//外设数据长度:32位（Word）
  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;//存储器数据长度:32位（Word）
  DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
  DMA_InitStructure.DMA_Priority = DMA_Priority_High;//高优先级
  DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;// 启用FIFO模式
  DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
  DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;// 存储器突发4次传输
  DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;// 外设突发4次传输
  DMA_Init(DMA2_Stream1, &DMA_InitStructure);//初始化DMA Stream
	
} 

//DCMI初始化
void My_DCMI_Init(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOC|RCC_AHB1Periph_GPIOE, ENABLE);//使能GPIOA B C E 时钟
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_DCMI,ENABLE);//使能DCMI时钟
  //PA4/6初始化设置
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4|GPIO_Pin_6;//PA4/6   复用功能
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF; //复用功能
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
  GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化
	
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7|GPIO_Pin_6;// PB6/7   复用功能
  GPIO_Init(GPIOB, &GPIO_InitStructure);//初始化
	
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_11;//PC6/7/8/9/11  复用功能
  GPIO_Init(GPIOC, &GPIO_InitStructure);//初始化	

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5|GPIO_Pin_6;//PE5/6  复用功能 
  GPIO_Init(GPIOE, &GPIO_InitStructure);//初始化	

	GPIO_PinAFConfig(GPIOA,GPIO_PinSource4,GPIO_AF_DCMI); //PA4,AF13  DCMI_HSYNC
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource6,GPIO_AF_DCMI); //PA6,AF13  DCMI_PCLK  
 	GPIO_PinAFConfig(GPIOB,GPIO_PinSource7,GPIO_AF_DCMI); //PB7,AF13  DCMI_VSYNC 
 	GPIO_PinAFConfig(GPIOC,GPIO_PinSource6,GPIO_AF_DCMI); //PC6,AF13  DCMI_D0  
 	GPIO_PinAFConfig(GPIOC,GPIO_PinSource7,GPIO_AF_DCMI); //PC7,AF13  DCMI_D1 
	GPIO_PinAFConfig(GPIOC,GPIO_PinSource8,GPIO_AF_DCMI); //PC8,AF13  DCMI_D2
	GPIO_PinAFConfig(GPIOC,GPIO_PinSource9,GPIO_AF_DCMI); //PC9,AF13  DCMI_D3
	GPIO_PinAFConfig(GPIOC,GPIO_PinSource11,GPIO_AF_DCMI);//PC11,AF13 DCMI_D4 
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource6,GPIO_AF_DCMI); //PB6,AF13  DCMI_D5 
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource5,GPIO_AF_DCMI); //PE5,AF13  DCMI_D6
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource6,GPIO_AF_DCMI); //PE6,AF13  DCMI_D7

	
	DCMI_DeInit();//还原默认配置 
 
 
  DCMI_InitStructure.DCMI_CaptureMode=DCMI_CaptureMode_Continuous;
	DCMI_InitStructure.DCMI_CaptureRate=DCMI_CaptureRate_All_Frame;//全帧捕获（参考成功案例配置）
	DCMI_InitStructure.DCMI_ExtendedDataMode= DCMI_ExtendedDataMode_8b;//8位数据格式  
	DCMI_InitStructure.DCMI_HSPolarity= DCMI_HSPolarity_Low;//HSYNC 低电平有效（成功案例配置）
	DCMI_InitStructure.DCMI_PCKPolarity= DCMI_PCKPolarity_Rising;//PCLK 上升沿有效（成功案例配置）
	DCMI_InitStructure.DCMI_SynchroMode= DCMI_SynchroMode_Hardware;//硬件同步HSYNC,VSYNC
	DCMI_InitStructure.DCMI_VSPolarity=DCMI_VSPolarity_High;//VSYNC 高电平有效
	DCMI_Init(&DCMI_InitStructure);

	DCMI_Cmd(ENABLE);//使能DCMI外设（成功案例配置）

	DCMI_ITConfig(DCMI_IT_FRAME,ENABLE);//使能帧中断 

  NVIC_InitStructure.NVIC_IRQChannel = DCMI_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2;//抢占优先级2
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =3;		//子优先级2
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
} 

//DCMI,开启捕获
void DCMI_Start(void)
{  
	DMA_Cmd(DMA2_Stream1, ENABLE);//使能DMA2,Stream1 
	DCMI_CaptureCmd(ENABLE);//DCMI捕获使能  
}

//DCMI,关闭捕获
void DCMI_Stop(void)
{ 
  DCMI_CaptureCmd(DISABLE);//DCMI捕获使能关闭	
	
	while(DCMI->CR&0X01);		//等待捕获结束 
	 	
	DMA_Cmd(DMA2_Stream1,DISABLE);//关闭DMA2,Stream1
}
