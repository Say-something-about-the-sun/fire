#include "ethernet_report.h"
#include "esp8266_report.h" // 借用你的传感器采集和 JSON 打包结构体与函数
#include <string.h>

// LwIP Socket 核心头文件
#include "lwip/sockets.h"
#include "lwip/opt.h"      // 引入 LwIP 配置


// 如果你的工程使用的是 Safe_Printf，可以保留；这里用标准 printf 演示
#include <stdio.h> 


#include "lwip_comm.h"



// --- 请替换为你实际的 OneNet 设备信息 ---
#define ONENET_MQTT_IP      "183.230.40.96"   // OneNet MQTT 服务器 IP (请 ping 你平台对应的域名)
#define ONENET_MQTT_PORT    1883             // OneNet 端口 (旧版通常是 6002，Studio版是 1883)


#define MQTT_CLIENT_ID      "stm32f407zgt6-networkport"      // 例如："123456789"
#define MQTT_USERNAME       "PGs73D47Zf"       // 例如："654321"
#define MQTT_PASSWORD       "version=2018-10-31&res=products%2FPGs73D47Zf%2Fdevices%2Fstm32f407zgt6-networkport&et=1805469231&method=sha1&sign=cUmFRKmVGbMg1dCPDIML4co%2FPYs%3D"     // APIKey 或者 Token

#define MQTT_TOPIC          "$sys/PGs73D47Zf/stm32f407zgt6-networkport/thing/property/post"

/*
#define MQTT_USERNAME  "PGs73D47Zf"
#define MQTT_CLIENT_ID   "stm32f407zgt6"
#define MQTT_PASSWORD  "version=2018-10-31&res=products%2FPGs73D47Zf%2Fdevices%2Fstm32f407zgt6&et=1805469231&method=sha1&sign=snpTD9lxQKVsom7omCk5XnSUCG8%3D"

#define MQTT_TOPIC          "$sys/PGs73D47Zf/stm32f407zgt6/thing/property/post"
*/

// 你 ESP8266 发往 OneNet 用的 Topic 是什么，这里就填什么！
// 例如旧版："dp", 新版 Studio 可能是："$sys/产品ID/设备名称/dp/post/json"



static void Ethernet_Build_Studio_JSON(SensorDataPacket* packet, JsonDataPacket* json);
static void USART3_Hardware_Sleep(void);
static void USART3_Hardware_Wakeup(void);


// =========================================================================
// 🚀 微型 MQTT 组装引擎 (Micro-MQTT) - 专为单次上报设计，零内存泄漏
// =========================================================================

// 辅助函数：将字符串写入缓冲区，并带上 2 字节的长度前缀
static int write_mqtt_string(u8 *buf, const char *str) {
    int len = strlen(str);
    buf[0] = (len >> 8) & 0xFF; // 长度高位
    buf[1] = len & 0xFF;        // 长度低位
    memcpy(&buf[2], str, len);  // 拷贝内容
    return len + 2;
}

// 辅助函数：计算并写入 MQTT 的“剩余长度 (Remaining Length)”
static int encode_remaining_length(u8 *buf, int len) {
    int pos = 0;
    do {
        u8 d = len % 128;
        len /= 128;
        if (len > 0) d |= 128;
        buf[pos++] = d;
    } while (len > 0);
    return pos; // 返回长度字节所占的空间 (1~4字节)
}

// 🎯 功能 1：组装 CONNECT (登录) 报文
// 返回值：组装后的报文总长度
int MicroMQTT_BuildConnect(const char *client_id, const char *username, const char *password, u8 *out_buf) {
    u8 payload[256];
    int p_len = 0;
    
    // 1. 组装 Payload (ClientID, Username, Password)
    p_len += write_mqtt_string(&payload[p_len], client_id);
    p_len += write_mqtt_string(&payload[p_len], username);
    p_len += write_mqtt_string(&payload[p_len], password);
    
    // 2. 组装可变报头 (Variable Header) - 固定为 MQTT 3.1.1 标准
    u8 var_header[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', // 协议名
        0x04,                           // 协议级别
        0xC2,                           // 标志位 (Clean Session=1, User=1, Pass=1)
        0x00, 0x3C                      // KeepAlive (60秒，反正我们发完就断，无所谓)
    };
    
    // 3. 拼接完整的 CONNECT 报文
    int total_len = 0;
    out_buf[total_len++] = 0x10; // 固定报头：0x10 表示 CONNECT
    total_len += encode_remaining_length(&out_buf[total_len], sizeof(var_header) + p_len);
    memcpy(&out_buf[total_len], var_header, sizeof(var_header)); total_len += sizeof(var_header);
    memcpy(&out_buf[total_len], payload, p_len); total_len += p_len;
    
    return total_len;
}

// 🎯 功能 2：组装 PUBLISH (发布) 报文
// 返回值：组装后的报文总长度
int MicroMQTT_BuildPublish(const char *topic, const char *json_str, int json_len, u8 *out_buf) {
    int total_len = 0;
    int topic_len = strlen(topic);
    
    // 剩余长度 = 2字节(Topic长度) + Topic字符串长度 + JSON长度
    int remain_len = 2 + topic_len + json_len;
    
    out_buf[total_len++] = 0x30; // 固定报头：0x30 表示 PUBLISH, QoS=0
    total_len += encode_remaining_length(&out_buf[total_len], remain_len);
    
    // 写入 Topic
    out_buf[total_len++] = (topic_len >> 8) & 0xFF;
    out_buf[total_len++] = topic_len & 0xFF;
    memcpy(&out_buf[total_len], topic, topic_len); total_len += topic_len;
    
    // 写入 JSON 实体
    memcpy(&out_buf[total_len], json_str, json_len); total_len += json_len;
    
    return total_len;
}


void Ethernet_Report_SendSensorData(void)
{
    static SensorDataPacket sensor_packet;
    static JsonDataPacket json_packet; 
    static u8 mqtt_buffer[512]; 
    int packet_len;
    int sock;
    struct sockaddr_in server_addr;

    Safe_Printf("\r\n[WARN] Main link failed! Triggering Ethernet failover...\r\n");
    
    // ==========================================================
    // 🚀 终极护航：全员静默，释放总线与电源极限！
    // ==========================================================
    // 1. 关停 ESP32(USART2) 和 ESP8266(USART3) 的中断
    // 绝对防止在网卡处理 TCP 握手时，被外来的串口乱码打断产生 ORE 溢出死锁！
    NVIC_DisableIRQ(USART2_IRQn); 
    NVIC_DisableIRQ(USART3_IRQn); 
    
    // 2. 停掉 DCMI 外设和 DMA，把 AHB 总线的带宽 100% 让给以太网
    DCMI_Stop(); 
    
    // 3. 🚨 强行让 OV5640 摄像头进入深度休眠！(极其关键)
    // 寄存器 0x3008 的 bit 6 设置为 1 (0x42) 会让传感器进入待机模式，功耗骤降，拯救 3.3V 稳压芯片！
    OV5640_WR_Reg(0x3008, 0x42); 
    
    // 4. 屏息等待 300ms：让车祸残骸排空，让主板的 3.3V 电压彻底回血升满！
    vTaskDelay(300 / portTICK_PERIOD_MS); 


    // ==========================================================
    // 🌐 网卡纯净拨号时间
    // ==========================================================
    Safe_Printf("-> [Ethernet] IP: %d.%d.%d.%d, Gateway: %d.%d.%d.%d\r\n", 
           lwipdev.ip[0], lwipdev.ip[1], lwipdev.ip[2], lwipdev.ip[3],
           lwipdev.gateway[0], lwipdev.gateway[1], lwipdev.gateway[2], lwipdev.gateway[3]);
           
    Safe_Printf("-> [Ethernet] Activating backup link (MQTT Passthrough)...\r\n");
    
    ESP8266_Report_CollectSensorData(&sensor_packet);
    Ethernet_Build_Studio_JSON(&sensor_packet, &json_packet);
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        Safe_Printf("-> [Ethernet Error] Socket creation failed!\r\n");
        goto RESTORE_SYSTEM; // 失败也必须跳到末尾恢复摄像头！
    }
    
    int timeout = 1000; 
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ONENET_MQTT_PORT); 
    server_addr.sin_addr.s_addr = inet_addr(ONENET_MQTT_IP); 

    int res = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (res < 0) {
        Safe_Printf("-> [Ethernet Error] Connect failed, error code: %d\r\n", res);
        close(sock);
        goto RESTORE_SYSTEM; // 失败也必须跳到末尾恢复摄像头！
    }

    Safe_Printf("-> [Ethernet] TCP Connected Successfully!\r\n");

    // 打包并发送 MQTT 数据
    packet_len = MicroMQTT_BuildConnect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, mqtt_buffer);
    send(sock, mqtt_buffer, packet_len, 0);
    
    vTaskDelay(50 / portTICK_PERIOD_MS); 
    
    packet_len = MicroMQTT_BuildPublish(MQTT_TOPIC, json_packet.json_str, json_packet.json_len, mqtt_buffer);
    int send_bytes = send(sock, mqtt_buffer, packet_len, 0);
    
    if(send_bytes > 0) {
        Safe_Printf("-> [Ethernet] MQTT payload delivered to Cloud! (%d bytes)\r\n", json_packet.json_len);
    }
    
    // 等待云端回执
    u8 rx_buf[64];
    int rx_len = recv(sock, rx_buf, sizeof(rx_buf) - 1, 0);
    if(rx_len > 0) {
        Safe_Printf("-> [Ethernet] Cloud ACK received! Perfect loop!\r\n");
    } else {
        Safe_Printf("-> [Ethernet Warn] Data sent, but no Cloud ACK received.\r\n");
    }
    
    close(sock); 
    Safe_Printf("-> [Ethernet] Session closed. Pipeline secured.\r\n");


    // ==========================================================
    // 🚀 拨号完成：系统状态重置，满血复活！
    // ==========================================================
RESTORE_SYSTEM:
    // 1. 唤醒 OV5640：写入 0x02 恢复正常工作输出
    OV5640_WR_Reg(0x3008, 0x02); 
    
    // 2. 必须给摄像头内部 ISP 和时钟留出启动重启的时间
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    
    // 3. 重新开启 DCMI 总线捕获，恢复画面
    DCMI_Start(); 
    
    // 4. 清理在静默期间，两个串口积攒在接收寄存器里的垃圾错乱数据
    // 这一步能彻底拔掉 ORE 溢出死锁的隐患！
    USART_ReceiveData(USART2); 
    USART_ClearFlag(USART2, USART_FLAG_ORE);
    USART_ReceiveData(USART3); 
    USART_ClearFlag(USART3, USART_FLAG_ORE);
    
    // 5. 重新开放串口中断，恢复 ESP32 与 ESP8266 的通信
    NVIC_EnableIRQ(USART2_IRQn); 
    NVIC_EnableIRQ(USART3_IRQn); 
}

		
  




// =========================================================================
// 🚀 OneNet Studio 专属 JSON 打包器 (终极完美对账版)
// =========================================================================
static void Ethernet_Build_Studio_JSON(SensorDataPacket* packet, JsonDataPacket* json)
{
    // 1. 【防 FPU 死机】：手动拆分所有浮点数
    int smoke_int = (int)packet->smoke_ppm;
    int smoke_dec = (int)((packet->smoke_ppm - smoke_int) * 100);
    if(smoke_dec < 0) smoke_dec = -smoke_dec;

    int temp_int = (int)packet->temperature;
    int temp_dec = (int)((packet->temperature - temp_int) * 10);
    if(temp_dec < 0) temp_dec = -temp_dec;
    
    int hum_int = (int)packet->humidity;
    int hum_dec = (int)((packet->humidity - hum_int) * 10);
    if(hum_dec < 0) hum_dec = -hum_dec;
    
    // 🚨 修正 1：获取虚拟电流值，准备填入 "current"
    int cur_int = (int)packet->virtual_current;
    int cur_dec = (int)((packet->virtual_current - cur_int) * 100);
    if(cur_dec < 0) cur_dec = -cur_dec;

    memset(json->json_str, 0, sizeof(json->json_str));

    // 2. 🚨 修正 2：严格遵守 bool 类型发 %s (true/false) 的铁律！
    int len = snprintf(json->json_str, sizeof(json->json_str),
        "{"
        "\"id\":\"123\","
        "\"version\":\"1.0\","
        "\"params\":{"
        
        // --- Float 单精度浮点型 ---
        "\"temperature\":{\"value\":%d.%01d},"
        "\"humidity\":{\"value\":%d.%01d},"
        "\"smoke_ppm\":{\"value\":%d.%02d},"
        "\"current\":{\"value\":%d.%02d},"  // 名字已改为 current
        
        // --- Int32 和 Enum 整数型 ---
        "\"flame_adc\":{\"value\":%d},"
        "\"temp_detected\":{\"value\":%d},"
        "\"risk_level\":{\"value\":%d},"
        "\"system_mode\":{\"value\":%d},"
        
        // --- Bool 布尔型 (极其严格，必须用 %s 填入 true/false) ---
        "\"pump_status\":{\"value\":%s},"
        "\"main_power_status\":{\"value\":%s}"
        
        "}" // params 结束
        "}",// 最外层结束
        
        // 对应 Float 值
        temp_int, temp_dec,
        hum_int, hum_dec,
        smoke_int, smoke_dec,
        cur_int, cur_dec,
        
        // 对应 Int32 / Enum 值
        packet->flame_adc,
        packet->temp_detected,
        packet->risk_level,
        packet->system_mode,
        
        // 对应 Bool 值 (动态转换为字符串)
        packet->pump_status ? "true" : "false",
        packet->main_power_status ? "true" : "false"
    );
    
    json->json_len = (len > 0) ? (u16)len : 0;
}



// =======================================================
// 🚀 工业级防线：彻底屏蔽故障模块的电磁干扰
// =======================================================
static void USART3_Hardware_Sleep(void)
{
    // 1. 核心级屏蔽中断！直接在 ARM 内核层面把 USART3 的中断线剪断！
    NVIC_DisableIRQ(USART3_IRQn);
    
    // 2. 关闭串口硬件状态机
    USART_Cmd(USART3, DISABLE);
}

static void USART3_Hardware_Wakeup(void)
{
    // 1. 疯狂读取数据寄存器，把断网期间积压在里面的物理电磁乱码全部倒进垃圾桶
    USART_ReceiveData(USART3);
    USART_ReceiveData(USART3);
    
    // 2. 暴力清除所有错误标志位 (特别是 ORE 溢出错误)
    USART_ClearFlag(USART3, USART_FLAG_ORE | USART_FLAG_RXNE | USART_FLAG_PE | USART_FLAG_FE);
    
    // 3. 重启串口硬件
    USART_Cmd(USART3, ENABLE);
    
    // 4. 清理干净后，重新接通 ARM 内核的中断线
    NVIC_EnableIRQ(USART3_IRQn);
}


/*
static void Ethernet_Build_Studio_JSON(SensorDataPacket* packet, JsonDataPacket* json)
{
    memset(json->json_str, 0, sizeof(json->json_str));

    // 🚀 极简测试法：只发一个绝对不会错的假温度！
    // ⚠️ 前提：请确保你的 OneNet 物模型里，确实有一个叫 "temperature" 的属性，且类型是 float 或 double！
    int len = snprintf(json->json_str, sizeof(json->json_str),
        "{"
        "\"id\":\"123\","
        "\"version\":\"1.0\","
        "\"params\":{"
        "\"temperature\":{\"value\":25.5}" // 强制写死一个小数
        "}"
        "}"
    );
    
    json->json_len = (len > 0) ? (u16)len : 0;
}
*/
