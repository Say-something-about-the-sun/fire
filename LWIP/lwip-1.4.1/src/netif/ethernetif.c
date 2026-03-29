#include "netif/ethernetif.h" 
#include "lan8720.h"  
#include "lwip_comm.h" 
#include "netif/etharp.h"  
#include "string.h" 
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include <lwip/lwip_sys.h>
//#inlcude "debug.h"
//这是个网卡驱动的C文件
xSemaphoreHandle s_xSemaphore;
xSemaphoreHandle t_xSemaphore;
extern struct netif lwip_netif;	
xTaskHandle Thread_intTask_Handler;
xTaskHandle Thread_outTask_Handler;
sys_sem_t tx_sem = NULL;
sys_mbox_t eth_tx_mb = NULL;
//由ethernetif_init()调用用于初始化硬件
//netif:网卡结构体指针 
//返回值:ERR_OK,正常
//       其他,失败
static err_t low_level_init(struct netif *netif)
{
#ifdef CHECKSUM_BY_HARDWARE
	int i; 
#endif 
	netif->hwaddr_len = ETHARP_HWADDR_LEN; //设置MAC地址长度,为6个字节
	//初始化MAC地址,设置什么地址由用户自己设置,但是不能与网络中其他设备MAC地址重复
	netif->hwaddr[0]=lwipdev.mac[0]; 
	netif->hwaddr[1]=lwipdev.mac[1]; 
	netif->hwaddr[2]=lwipdev.mac[2];
	netif->hwaddr[3]=lwipdev.mac[3];
	netif->hwaddr[4]=lwipdev.mac[4];
	netif->hwaddr[5]=lwipdev.mac[5];
	netif->mtu=1500; //最大允许传输单元,允许该网卡广播和ARP功能

	netif->flags = NETIF_FLAG_BROADCAST|NETIF_FLAG_ETHARP|NETIF_FLAG_LINK_UP;
	
	ETH_MACAddressConfig(ETH_MAC_Address0, netif->hwaddr); //向STM32F4的MAC地址寄存器中写入MAC地址
	/* 将发送描述符与接收描述符组成链接结构，并给每个描述符分配缓存区地址*/
	ETH_DMATxDescChainInit(DMATxDscrTab, Tx_Buff, ETH_TXBUFNB);
	ETH_DMARxDescChainInit(DMARxDscrTab, Rx_Buff, ETH_RXBUFNB);
	/* ******** */
#ifdef CHECKSUM_BY_HARDWARE 	//使用硬件帧校验
	for(i=0;i<ETH_TXBUFNB;i++)	//使能TCP,UDP和ICMP的发送帧校验,TCP,UDP和ICMP的接收帧校验在DMA中配置了
	{
		ETH_DMATxDescChecksumInsertionConfig(&DMATxDscrTab[i], ETH_DMATxDesc_ChecksumTCPUDPICMPFull);
	}
#endif
///////////////////////////////////////////////////////////////////////	
	  s_xSemaphore=xSemaphoreCreateCounting(40,0);
	  t_xSemaphore=xSemaphoreCreateMutex();
//	  s_xSemaphore=xSemaphoreCreateBinary();
//  if(sys_sem_new(&tx_sem , 0) == ERR_OK)
//    printf("sys_sem_new ok\n");
////  
//  if(sys_mbox_new(&eth_tx_mb , 50) == ERR_OK)
//    printf("sys_mbox_new ok\n");
	
	
   	xTaskCreate((TaskFunction_t )ethernetif_input,            //任务函数
              (const char*    )"ethernetif_input",          //任务名称
              (uint16_t       )1024,        //任务堆栈大小
              (void*          )netif,                  //传递给任务函数的参数
              (UBaseType_t    )30,       //任务优先级
              (TaskHandle_t*  )&Thread_intTask_Handler); 
			
							
//		xTaskCreate((TaskFunction_t )ethernetif_output,            //任务函数
//              (const char*    )"ethernetif_output",          //任务名称
//              (uint16_t       )1024,        //任务堆栈大小
//              (void*          )netif,                  //传递给任务函数的参数
//              (UBaseType_t    )31,       //任务优先级
//              (TaskHandle_t*  )&Thread_outTask_Handler); 					
     //	sys_thread_new("ETHTX",
//                  ethernetif_output,  /* 任务入口函数 */
//                  netif,        	  /* 任务入口函数参数 */
//                  NETIF_OUT_TASK_STACK_SIZE,/* 任务栈大小 */
//                  NETIF_OUT_TASK_PRIORITY); /* 任务的优先级 */
							
//		sys_thread_new("ethernetif_input",
//                  ethernetif_input,  /* 任务入口函数 */
//                  NULL,        	  /* 任务入口函数参数 */
//                  2048,/* 任务栈大小 */
//                  29); /* 任务的优先级 */
//////////////////////////////////////////////////////////////////////////////	
	ETH_Start(); //开启MAC和DMA				
	return ERR_OK;
} 
//用于发送数据包的最底层函数(lwip通过netif->linkoutput指向该函数)
//netif:网卡结构体指针
//p:pbuf数据结构体指针
//返回值:ERR_OK,发送正常
//       ERR_MEM,发送失败
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	u8 res;
	struct pbuf *q;
	int l = 0;
	u8 *buffer=(u8 *)ETH_GetCurrentTxBuffer(); //获取当前要发送的DMA描述符中的缓冲区地址
	/*将p(LWIP数据包)拷贝到发送描述符缓存区中*/
	for(q=p;q!=NULL;q=q->next) 
	{
		memcpy((u8_t*)&buffer[l], q->payload, q->len);
		l=l+q->len;
	} 
	/* ******** */
	res=ETH_Tx_Packet(l); //调用ETH_Tx_Packet函数发送数据
	if(res==ETH_ERROR)return ERR_MEM;//返回错误状态
	return ERR_OK;
}  
///用于接收数据包的最底层函数
//neitif:网卡结构体指针
//返回值:pbuf数据结构体指针
static struct pbuf * low_level_input(struct netif *netif)
{  
	struct pbuf *p, *q;
	u16_t len;
	int l =0;
	FrameTypeDef frame;
	u8 *buffer;
	p = NULL;
	frame=ETH_Rx_Packet();
	len=frame.length;//得到包大小
	buffer=(u8 *)frame.buffer;//得到包数据地址 
	p=pbuf_alloc(PBUF_RAW,len,PBUF_POOL);//pbufs内存池分配pbuf
	/*将接收描述符缓存区中的数据拷贝到LWIP数据结构体（这是LWIP的数据包格式）中*/
	if(p!=NULL)
	{
		for(q=p;q!=NULL;q=q->next)
		{
			memcpy((u8_t*)q->payload,(u8_t*)&buffer[l], q->len);
			l=l+q->len;
		}    
	}
	/* ******** */
	frame.descriptor->Status=ETH_DMARxDesc_OWN;//设置Rx描述符OWN位,buffer重归ETH DMA 
	if((ETH->DMASR&ETH_DMASR_RBUS)!=(u32)RESET)//当Rx Buffer不可用位(RBUS)被设置的时候,重置它.恢复传输
	{ 
		ETH->DMASR=ETH_DMASR_RBUS;//重置ETH DMA RBUS位 
		ETH->DMARPDR=0;//恢复DMA接收
	}
	return p;
}
//网卡接收数据(lwip直接调用)
//netif:网卡结构体指针
//返回值:ERR_OK,发送正常
//       ERR_MEM,发送失败
//err_t ethernetif_input(struct netif *netif)
//{
//	 uint32_t ulReturn;
//	err_t err;
//	struct pbuf *p;
//	ulReturn = taskENTER_CRITICAL_FROM_ISR();
//	p=low_level_input(netif);   //调用low_level_input函数接收数据
//	if(p==NULL) return ERR_MEM;
//	err=netif->input(p, netif); //调用netif结构体中的input字段(一个函数)来处理数据包
//	taskEXIT_CRITICAL_FROM_ISR( ulReturn );	
//	if(err!=ERR_OK)
//	{
//		LWIP_DEBUGF(NETIF_DEBUG,("ethernetif_input: IP input error\n"));
//		pbuf_free(p);
//		p = NULL;
//	} 
//	return err;
//}


//网卡接收数据(lwip直接调用)
//netif:网卡结构体指针
//返回值:ERR_OK,发送正常
//       ERR_MEM,发送失败


//extern u8 xxflag;
void ethernetif_input(void *pvParameters)
{
	
	struct netif *netif;
	struct pbuf *p = NULL;
	netif = (struct netif*) pvParameters;
  LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
	while(1)
	{
			 if(xSemaphoreTake( s_xSemaphore, portMAX_DELAY ) == pdTRUE)
			 {
				taskENTER_CRITICAL();
			TRY_GET_NEXT_FRAGMENT:
				p=low_level_input(netif);   //调用low_level_input函数接收数据
				taskEXIT_CRITICAL();
				
				 if(p != NULL)
				 {
							taskENTER_CRITICAL();
							/* full packet send to tcpip_thread to process */
							if (netif->input(p, netif) != ERR_OK)
							{
								LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
								pbuf_free(p);
								p = NULL;
							}
							else
							{
								xSemaphoreTake(s_xSemaphore, 0);
//								p = NULL;
								goto TRY_GET_NEXT_FRAGMENT;
							}
							taskEXIT_CRITICAL();
					}
				}
   }

} 


//void ethernetif_output(void *pvParameters) 
//{
//  void *msg;
//  struct pbuf *p;
//  struct netif *netif;  
//  netif = (struct netif*) pvParameters;
//  while(1)
//  {
//    if(sys_arch_mbox_fetch(&eth_tx_mb,&msg,0)==ERR_OK)
//    {
//      printf("sys_arch_mbox_fetch ok\n");
//      p = (struct pbuf *)msg;
//      
//      if(p!=NULL)
//      {
//        if(low_level_output(netif,p) == ERR_OK)
//        {
//          printf("low_level_output ok\n");
////          LED3_TOGGLE;
//        }
//      }
//      
//      /* send ACK */
//      sys_sem_signal(&tx_sem);
//    }
//    else
//      printf("not ok\n");
//  }
//  
//}







//使用low_level_init()函数来初始化网络
//netif:网卡结构体指针
//返回值:ERR_OK,正常
//       其他,失败
err_t ethernetif_init(struct netif *netif)
{
	LWIP_ASSERT("netif!=NULL",(netif!=NULL));
#if LWIP_NETIF_HOSTNAME			//LWIP_NETIF_HOSTNAME 
	netif->hostname="lwip";  	//初始化名称
#endif 
	netif->name[0]=IFNAME0; 	//初始化变量netif的name字段
	netif->name[1]=IFNAME1; 	//在文件外定义这里不用关心具体值
	netif->output=etharp_output;//IP层发送数据包函数
	netif->linkoutput=low_level_output;//ARP模块发送数据包函数
	low_level_init(netif); 		//底层硬件初始化函数
	return ERR_OK;
}
