#include "ethernet_report.h"
#include "esp8266_report.h" // 借用你的传感器采集和 JSON 打包结构体与函数

// LwIP Socket 核心头文件
#include "lwip/sockets.h"
#include "lwip/opt.h"      // 引入 LwIP 配置


// 如果你的工程使用的是 Safe_Printf，可以保留；这里用标准 printf 演示
#include <stdio.h> 


/*
void Ethernet_Report_SendSensorData(void)
{
    // 1. 静态分配，防止爆掉 FreeRTOS 任务栈
    static SensorDataPacket sensor_packet;
    static JsonDataPacket json_packet; 
    
    int sock = -1;
    struct sockaddr_in server_addr;
    int send_bytes = 0;

    printf("-> [Ethernet] 激活备用以太网链路，准备上报数据...\r\n");
    
    // 2. 数据采集与打包 (完全白嫖 WiFi 链路的底层函数！)
    ESP8266_Report_CollectSensorData(&sensor_packet);
    ESP8266_Report_PackageJSON(&sensor_packet, &json_packet);
    
    // 如果打包失败或没有数据，直接退出
    if (json_packet.json_len == 0) {
        printf("-> [Ethernet] 无有效 JSON 数据，取消发送。\r\n");
        return;
    }

    // ==========================================
    // 3. LwIP 网络发送核心逻辑 (UDP 客户端模式)
    // ==========================================
    
    // 3.1 申请一个 UDP Socket
    // AF_INET: IPv4, SOCK_DGRAM: UDP协议
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) 
    {
        printf("-> [Ethernet Error] 申请 Socket 失败！LwIP 资源可能不足。\r\n");
        return; 
    }

    // 3.2 配置目标服务器（电脑）的 IP 和端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BACKUP_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(BACKUP_SERVER_IP);

		
		struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000; // 500ms
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
		
		
		
		
		
    // 3.3 执行发送
    // 参数：socket句柄, 数据指针, 数据长度, 标志位, 目标地址结构体, 结构体大小
    send_bytes = sendto(sock, 
                        json_packet.json_str, 
                        json_packet.json_len, 
                        0, 
                        (struct sockaddr *)&server_addr, 
                        sizeof(server_addr));

    // 3.4 检查发送结果
    if (send_bytes > 0) 
    {
        printf("-> [Ethernet] 备用链路 JSON 发送成功！(%d 字节)\r\n", send_bytes);
        // 可选：你甚至可以把发出去的 JSON 内容打印出来看看
        // printf("%s\r\n", json_packet.json_str); 
    } 
    else 
    {
        printf("-> [Ethernet Error] UDP 数据包发送失败！\r\n");
    }

    // 3.5 极其重要：发送完毕后，必须关闭 Socket，释放 LwIP 内核资源！
    // 否则发几次之后系统就因为 Socket 耗尽而死锁了。
    close(sock); 
}

*/


void Ethernet_Report_SendSensorData(void)
{
    int sock = -1;
    struct sockaddr_in server_addr;
    
    // 🚨 1. 把这两行真实的采集和打包代码【注释掉】！
    // ESP8266_Report_CollectSensorData(&sensor_packet);
    // ESP8266_Report_PackageJSON(&sensor_packet, &json_packet);
    
    // 🚨 2. 直接伪造一个死数据包
    char* test_str = "{\"Status\":\"Alive\"}";
    int test_len = 18;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("192.168.0.101");

    // 🚨 3. 发送假数据
    int send_bytes = sendto(sock, test_str, test_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    
    if(send_bytes > 0) printf("-> UDP OK!\r\n");

    close(sock);
}

