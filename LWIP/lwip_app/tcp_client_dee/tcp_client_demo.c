#include "tcp_client_demo.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
#include "FreeRTOS.h"
#include "task.h"

static void client(void *thread_param)
{
  struct netconn *conn;
  int ret;
  ip4_addr_t ipaddr;
  
  uint8_t send_buf[]= "This is a TCP Client test...\n";
  
  printf("目地IP地址:%d.%d.%d.%d \t 端口号:%d\n\n",      \
          lwipdev.remoteip[0],lwipdev.remoteip[1],lwipdev.remoteip[2],lwipdev.remoteip[3],REMOTE_PORT);
  
  printf("请将电脑上位机设置为TCP Server.在User/arch/sys_arch.h文件中将目标IP地址修改为您电脑上的IP地址\n\n");
  
  printf("修改对应的宏定义:DEST_IP_ADDR0,DEST_IP_ADDR1,DEST_IP_ADDR2,DEST_IP_ADDR3,DEST_PORT\n\n");
  
  while(1)
  {
    conn = netconn_new(NETCONN_TCP);
    if (conn == NULL)
    {
      PRINT_DEBUG("create conn failed!\n");
      vTaskDelay(10);
      continue;
    }
    
    IP4_ADDR(&ipaddr,lwipdev.remoteip[0],lwipdev.remoteip[1],lwipdev.remoteip[2],lwipdev.remoteip[3]);

    ret = netconn_connect(conn,&ipaddr,REMOTE_PORT);
    if (ret == -1)
    {
        PRINT_DEBUG("Connect failed!\n");
        netconn_close(conn);
        vTaskDelay(10);
        continue;
    }

    PRINT_DEBUG("Connect to server successful!\n");
     
    while (1)
    {
      ret = netconn_write(conn,send_buf,sizeof(send_buf),0);
   
      vTaskDelay(1000);
    }
  }

}

void client_init(void)
{
  sys_thread_new("client", client, NULL, 512, 7);
}



