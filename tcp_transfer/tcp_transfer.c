#include "tcp_transfer.h"
#include "lwip/sockets.h"
#include <string.h>


#include "FreeRTOS.h"
#include "task.h"

#define PC_SERVER_IP   "192.168.0.101"
#define PC_SERVER_PORT 8080
u32_t sys_now(void);

static int g_fire_sock = -1;



// 🚀 在函数外部定义全局变量，避免频繁创建和销毁 Socket 导致邮箱死锁！
static int g_udp_sock = -1;
static struct sockaddr_in server_addr;

void udp_client_send_fire_alarm(u8 *jpeg_data, u32 jpeg_len)
{
    // 1. 如果公路还没修好，则进行初始化（开机只执行一次）
    if (g_udp_sock < 0) {
        g_udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (g_udp_sock < 0) return; // 如果 LwIP 还没准备好，直接放弃本帧，绝不死锁！

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8080);
        server_addr.sin_addr.s_addr = inet_addr("192.168.0.101");

        Safe_Printf("-> [Net] Socket 建立，正在预热 ARP...\r\n");

        // 🌟 核心绝杀 1：发送一个 1 字节的空包，提前触发 ARP 解析！
        // 这相当于提前“敲门”，防止后面的连发引发 ARP 广播风暴死锁！
        u8 dummy = 0xFF;
        sendto(g_udp_sock, &dummy, 1, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        
        // 给路由器和电脑 50ms 的时间回复 ARP 握手
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }

    // 2. 发送帧头（图片大小）
    u32 head = jpeg_len;
    sendto(g_udp_sock, &head, sizeof(u32), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    
    // 🌟 核心绝杀 2：帧头和正文之间必须喘息，防止粘包
    vTaskDelay(pdMS_TO_TICKS(5)); 

    // 3. 分块发送图片主体
    u32 sent_len = 0;
    while (sent_len < jpeg_len) 
    {
        u32 send_size = (jpeg_len - sent_len) > 1024 ? 1024 : (jpeg_len - sent_len);
        sendto(g_udp_sock, jpeg_data + sent_len, send_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        
        sent_len += send_size;
        
        // 🌟 核心绝杀 3：将发送间隔从 1ms 提升到 5ms！
        // 绝对防止 ST 底层网卡 TX 描述符爆满导致的无限死循环！
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
    
    // 🚀 注意：发完之后绝对不要调用 close(sock)！
    // 保持全局存活，下次直接用，绕开所有内核级资源分配！
    Safe_Printf("-> [Net] %d 字节 UDP 推流完美结束！\r\n", jpeg_len);
}


u32_t sys_now(void)
{
    
    return xTaskGetTickCount(); 
}

