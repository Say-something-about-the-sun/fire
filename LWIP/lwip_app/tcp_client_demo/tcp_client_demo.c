#include "tcp_client_demo.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "malloc.h"	
//#include "client.h"
//#include "includes.h"
//#include "key.h"
//#include "FreeRTOS.h"
//#include "task.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//NETCONN API编程方式的TCP客户端测试代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2014/8/15
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//*******************************************************************************
//修改信息
//无
//////////////////////////////////////////////////////////////////////////////// 	  
#define  NETCONN_CLIENT    0
#define  SOCKET_CLIENT     1

struct netconn *tcp_clientconn;					//TCP CLIENT网络连接结构体
u8 tcp_client_recvbuf[TCP_CLIENT_RX_BUFSIZE];	//TCP客户端接收数据缓冲区
u8 *tcp_client_sendbuf="Explorer STM32F407 NETCONN TCP Client send data\r\n";	//TCP客户端发送数据缓冲区
u8 tcp_client_flag;		//TCP客户端数据发送标志位
int sock = -1;
uint8_t send_buf[]= "This is a TCP Client test...\n";

u8 std=0;

void tcp_client_thread(void *pvParameters);
static void soket_client(void *pvParameters);
static void soket_send(void *pvParameters);
void tcp_client_thread(void *pvParameters)
{
//	OS_CPU_SR cpu_sr;
	u32 data_len = 0;
	static u8 js=0;
	static u8 jt=0;
	struct pbuf *q;
	err_t err,recv_err;
	static ip_addr_t server_ipaddr,loca_ipaddr;
	static u16_t 		 server_port,loca_port;

	LWIP_UNUSED_ARG(pvParameters);
	server_port = REMOTE_PORT;
	IP4_ADDR(&server_ipaddr, lwipdev.remoteip[0],lwipdev.remoteip[1], lwipdev.remoteip[2],lwipdev.remoteip[3]);
	
	while (1) 
	{
		tcp_clientconn = netconn_new(NETCONN_TCP);  //创建一个TCP链接
		if (tcp_clientconn == NULL)
    {
      printf("create tcp_clientconn failed!\n");
      vTaskDelay(10);
//      continue;
    }
		err = netconn_connect(tcp_clientconn,&server_ipaddr,server_port);//连接服务器
		if(err != ERR_OK)  
		{
			netconn_delete(tcp_clientconn);
			vTaskDelay(10);
		} //返回值不等于ERR_OK,删除tcp_clientconn连接
		else if (err == ERR_OK)    //处理新连接的数据
		{ 
			struct netbuf *recvbuf;
			tcp_clientconn->recv_timeout = 10;
			netconn_getaddr(tcp_clientconn,&loca_ipaddr,&loca_port,1); //获取本地IP主机IP地址和端口号
			printf("连接上服务器%d.%d.%d.%d,本机端口号为:%d\r\n",lwipdev.remoteip[0],lwipdev.remoteip[1], lwipdev.remoteip[2],lwipdev.remoteip[3],loca_port);
			while(1)
			{
				  if(js==100)
				  {
					  js=0;
					  err = netconn_write(tcp_clientconn ,tcp_client_sendbuf,strlen((char*)tcp_client_sendbuf),NETCONN_COPY); //发送tcp_server_sentbuf中的数据
						if(err != ERR_OK)
				    {
	           printf("发送失败\r\n");
						 vTaskDelay(10);
						 jt++;
					  }
						tcp_client_flag &= ~LWIP_SEND_DATA;
				  }

					if(jt==5)
					{
						jt=0;
					  netconn_close(tcp_clientconn);
					  netconn_delete(tcp_clientconn);
					  printf("服务器%d.%d.%d.%d断开连接\r\n",lwipdev.remoteip[0],lwipdev.remoteip[1], lwipdev.remoteip[2],lwipdev.remoteip[3]);					
						break;
					}
					
				  js++;
				  vTaskDelay(10);
				if((recv_err = netconn_recv(tcp_clientconn,&recvbuf)) == ERR_OK)  //接收到数据
				{	

					taskENTER_CRITICAL();  
					memset(tcp_client_recvbuf,0,TCP_CLIENT_RX_BUFSIZE);  //数据接收缓冲区清零
					for(q=recvbuf->p;q!=NULL;q=q->next)  //遍历完整个pbuf链表
					{
						//判断要拷贝到TCP_CLIENT_RX_BUFSIZE中的数据是否大于TCP_CLIENT_RX_BUFSIZE的剩余空间，如果大于
						//的话就只拷贝TCP_CLIENT_RX_BUFSIZE中剩余长度的数据，否则的话就拷贝所有的数据
						if(q->len > (TCP_CLIENT_RX_BUFSIZE-data_len)) memcpy(tcp_client_recvbuf+data_len,q->payload,(TCP_CLIENT_RX_BUFSIZE-data_len));//拷贝数据
						else memcpy(tcp_client_recvbuf+data_len,q->payload,q->len);
						data_len += q->len;  	
						if(data_len > TCP_CLIENT_RX_BUFSIZE) break; //超出TCP客户端接收数组,跳出	
					}
					taskEXIT_CRITICAL(); 
					
					data_len=0;  //复制完成后data_len要清零。					
					printf("%s\r\n",tcp_client_recvbuf);
					netbuf_delete(recvbuf);
				}else if(recv_err == ERR_CLSD)  //关闭连接
				{
					netconn_close(tcp_clientconn);
					netconn_delete(tcp_clientconn);
					printf("服务器%d.%d.%d.%d断开连接\r\n",lwipdev.remoteip[0],lwipdev.remoteip[1], lwipdev.remoteip[2],lwipdev.remoteip[3]);
					break;
				}
			}
		}
	}


}



//static void soket_client(void *pvParameters)
//{
// 
//  struct sockaddr_in client_addr;
//  
//  static ip_addr_t ipaddr;
//  int rc;
//  uint8_t res_buf[20];
//  
//  printf("目地IP地址:%d.%d.%d.%d \t 端口号:%d\n\n",      \
//          lwipdev.remoteip[0],lwipdev.remoteip[1],lwipdev.remoteip[2],lwipdev.remoteip[3],REMOTE_PORT);
//  
//  printf("请将电脑上位机设置为TCP Server.在User/arch/sys_arch.h文件中将目标IP地址修改为您电脑上的IP地址\n\n");
//  
//  printf("修改对应的宏定义:DEST_IP_ADDR0,DEST_IP_ADDR1,DEST_IP_ADDR2,DEST_IP_ADDR3,DEST_PORT\n\n");
////	LWIP_UNUSED_ARG(pvParameters);
//  IP4_ADDR(&ipaddr, lwipdev.remoteip[0],lwipdev.remoteip[1], lwipdev.remoteip[2],lwipdev.remoteip[3]);
////  IP4_ADDR(&ipaddr,DEST_IP_ADDR0,DEST_IP_ADDR1,DEST_IP_ADDR2,DEST_IP_ADDR3);
//  while(1)
//  {
//    sock = socket(AF_INET, SOCK_STREAM, 0);
//    if (sock < 0)
//    {
//      printf("Socket error\n");
//      vTaskDelay(10);
//      continue;
//    } 

//    client_addr.sin_family = AF_INET;      
//    client_addr.sin_port = htons(REMOTE_PORT);   
//    client_addr.sin_addr.s_addr = ipaddr.addr;
//    memset(&(client_addr.sin_zero), 0, sizeof(client_addr.sin_zero));    

//    if (connect(sock, 
//               (struct sockaddr *)&client_addr, 
//                sizeof(struct sockaddr)) == -1) 
//    {
//        printf("Connect failed!\n");
//        closesocket(sock);
//        vTaskDelay(10);
//        continue;
//    }                                           
//    
//    printf("Connect to server successful!\n");
//		 std=1;
////		while(std)
////		{
////		  
////		  vTaskDelay(10);
////		
////		}
//    
//    while (1)
//    {
//      rc = recv(sock, res_buf, 20, 0);
//			if(rc>0)
//			{
//      printf("%s\n",res_buf);
//		
//			}
//      vTaskDelay(10);
//    }
//    
////    closesocket(sock);
//  }

//}



static void soket_send(void *pvParameters)
{
  



	
    while (1)
    {
			if(std==1)
			{
				
				if(write(sock,send_buf,sizeof(send_buf)) < 0)
				{
					std=0;
//					vTaskDelay(1000);
					closesocket(sock);
					 break;
				}
		  }
      vTaskDelay(1000);
			
    }
    
   
	
	
  
}

//创建TCP客户端线程
//返回值:0 TCP客户端创建成功
//		其他 TCP客户端创建失败
u8 tcp_client_init(void)
{
	sys_thread_t res;
	sys_thread_t ree;
	#if  NETCONN_CLIENT
 
//	  res=sys_thread_new("tcp_client_thread", tcp_client_thread, NULL, 300, 4);
	#elif  SOCKET_CLIENT
//	  res=sys_thread_new("soket_client", soket_client, NULL, 512, 9);  
//    ree=sys_thread_new("soket_send", soket_send, NULL, 200, 4); 	
	#endif
    if((res!=NULL)&&(ree!=NULL))							
       return 0;
    else		
	     return 1;
}

