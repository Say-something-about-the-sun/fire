#include "iperf_client.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
//#include "FreeRTOS.h"
//#include "task.h"
u8 send_buf[]= "This is a TCP Client test...\n";
struct netconn *tcp_clientconn;	
static void client(void *pvParameters);

static void client(void *pvParameters)
{
//
 
  err_t err,recv_err;
  static ip4_addr_t ipaddr;
  
  LWIP_UNUSED_ARG(pvParameters);
	IP4_ADDR(&ipaddr,lwipdev.remoteip[0],lwipdev.remoteip[1],lwipdev.remoteip[2],lwipdev.remoteip[3]);
  printf("目地IP地址:%d.%d.%d.%d \t 端口号:%d\n\n",lwipdev.remoteip[0],lwipdev.remoteip[1],lwipdev.remoteip[2],lwipdev.remoteip[3],REMOTE_PORT);
  
  printf("请将电脑上位机设置为TCP Server.在User/arch/sys_arch.h文件中将目标IP地址修改为您电脑上的IP地址\n\n");
  
  printf("修改对应的宏定义:DEST_IP_ADDR0,DEST_IP_ADDR1,DEST_IP_ADDR2,DEST_IP_ADDR3,DEST_PORT\n\n");
  
  while(1)
  {
    tcp_clientconn = netconn_new(NETCONN_TCP);
    if (tcp_clientconn == NULL)
    {
      PRINT_DEBUG("create conn failed!\n");
      vTaskDelay(10);
//      continue;
    }
    
    

    err = netconn_connect(tcp_clientconn,&ipaddr,REMOTE_PORT);
    if (err != ERR_OK)
    {
        PRINT_DEBUG("Connect failed!\n");
        netconn_close(tcp_clientconn);
        vTaskDelay(10);
//        continue;
    }

    PRINT_DEBUG("Connect to server successful!\n");
     
    while (1)
    {
      err = netconn_write(tcp_clientconn,send_buf,sizeof(send_buf),0);
   
      vTaskDelay(1000);
    }
  }

}

void client_init(void)
{
//	xTaskCreate( thread, ( signed portCHAR * ) name, stacksize, arg, prio, &CreatedTask );
	
	
  sys_thread_new("client", client, NULL, 512, 13);
}
