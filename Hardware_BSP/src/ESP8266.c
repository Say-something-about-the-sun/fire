/**
 * @file    ESP8266.c
 * @brief   ESP8266 WiFi 模块 AT 指令集控制驱动
 * @note    基于串口3解析 AT 响应序列，提供网络接入及透传管理能力。
 */
#include "ESP8266.h"
#include "usart3.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

extern u8 USART3_RX_BUF[];
extern u16 USART3_RX_STA;

/**
 * @brief  发送 AT 指令并等待预期响应
 * @param  cmd               待发送的 AT 字符串
 * @param  expected_response 预期的应答子串（如 "OK"）
 * @param  timeout_ms        超时时间（毫秒）
 * @return ESP8266_Status    协议栈执行状态
 */
ESP8266_Status ESP8266_SendATCommand(char* cmd, char* expected_response, u32 timeout_ms)
{
    u32 start_time;
    
    printf("[ESP8266] Sending: %s", cmd);
    
    USART3_RX_STA = 0;
    memset(USART3_RX_BUF, 0, USART3_RX_BUF_SIZE);
    
    USART3_Send_String(cmd);
    
    start_time = 0;
    while(start_time < timeout_ms)
    {
        if(USART3_RX_STA > 0)
        {
            // 匹配到了目标字符
            if(strstr((char*)USART3_RX_BUF, expected_response) != NULL)
            {
                USART3_RX_STA = 0; 
                memset(USART3_RX_BUF, 0, USART3_RX_BUF_SIZE);
                return ESP8266_OK;
            }
            // 匹配到了错误标识
            else if(strstr((char*)USART3_RX_BUF, "ERROR") != NULL)
            {
                USART3_RX_STA = 0;
                memset(USART3_RX_BUF, 0, USART3_RX_BUF_SIZE);
                return ESP8266_ERROR;
            }
        }
        delay_ms(10);
        start_time += 10;
    }
    
    printf("[ESP8266] Timeout! Last buffer: %s\r\n", (char*)USART3_RX_BUF);
    USART3_RX_STA = 0;
    memset(USART3_RX_BUF, 0, USART3_RX_BUF_SIZE);
    return ESP8266_TIMEOUT;
}

/**
 * @brief  模块基础测试与工作模式初始化
 */
void ESP8266_Init(void)
{
    printf("[ESP8266] Initializing...\r\n");
    
    printf("[ESP8266] RX buffer address: 0x%08X\r\n", (u32)USART3_RX_BUF);
    printf("[ESP8266] RX status: 0x%04X\r\n", USART3_RX_STA);
    
    // 1. 测试基础通信
    printf("[ESP8266] Step 1: Testing AT command...\r\n");
    if(ESP8266_CheckStatus() != ESP8266_OK)
    {
        printf("[ESP8266] Initialization failed - No response\r\n");
        printf("[ESP8266] Please check:\r\n");
        printf("[ESP8266] 1. Hardware connection (PB10/PB11)\r\n");
        printf("[ESP8266] 2. ESP8266 power supply\r\n");
        printf("[ESP8266] 3. ESP8266 firmware\r\n");
        return;
    }
    
    // 2. 设定为Station(客户端)工作模式
    printf("[ESP8266] Step 2: Setting Station mode...\r\n");
    if(ESP8266_SetMode(1) != ESP8266_OK)
    {
        printf("[ESP8266] Failed to set mode\r\n");
        return;
    }
    
    // 3. 关闭回显防止缓冲池污染
    printf("[ESP8266] Step 3: Disabling echo...\r\n");
    ESP8266_SendATCommand("ATE0\r\n", "OK", 1000);
    
    // 4. 配置链路模式
    printf("[ESP8266] Step 4: Enabling single connection mode...\r\n");
    ESP8266_SetMuxMode(0);
    
    printf("[ESP8266] Initialization completed\r\n");
}

ESP8266_Status ESP8266_CheckStatus(void)
{
    return ESP8266_SendATCommand("AT\r\n", "OK", 1000);
}

ESP8266_Status ESP8266_SetMode(u8 mode)
{
    char cmd[32];
    sprintf(cmd, "AT+CWMODE=%d\r\n", mode);
    return ESP8266_SendATCommand(cmd, "OK", 1000);
}

/**
 * @brief  发起 WLAN 网络连接
 */
ESP8266_Status ESP8266_ConnectWiFi(char* ssid, char* password)
{
    char cmd[128];
    
    printf("[ESP8266] Connecting to WiFi: %s\r\n", ssid);
    printf("[ESP8266] WiFi password length: %d\r\n", strlen(password));
    
    ESP8266_DisconnectWiFi();
    delay_ms(1000);
    
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
    printf("[ESP8266] Sending command: %s", cmd);
    
    ESP8266_Status status = ESP8266_SendATCommand(cmd, "WIFI CONNECTED", 30000);
    
    if(status == ESP8266_OK)
    {
        printf("[ESP8266] WiFi connected successfully\r\n");
    }
    else
    {
        printf("[ESP8266] WiFi connection failed\r\n");
        printf("[ESP8266] Response: %s\r\n", (char*)USART3_RX_BUF);
    }
    
    return status;
}

ESP8266_Status ESP8266_DisconnectWiFi(void)
{
    return ESP8266_SendATCommand("AT+CWQAP\r\n", "OK", 2000);
}

ESP8266_Status ESP8266_EnableDHCP(void)
{
    printf("[ESP8266] Enabling DHCP...\r\n");
    ESP8266_Status status = ESP8266_SendATCommand("AT+CWDHCP=1,1\r\n", "OK", 2000);
    
    if(status == ESP8266_OK)
    {
        printf("[ESP8266] DHCP enabled successfully\r\n");
    }
    else
    {
        printf("[ESP8266] Failed to enable DHCP\r\n");
    }
    return status;
}

/**
 * @brief  创建 TCP/UDP 传输连接
 */
ESP8266_Status ESP8266_ConnectServer(char* mode, char* ip, u16 port)
{
    char cmd[128];
    printf("[ESP8266] Connecting to server: %s:%d\r\n", ip, port);
    
    sprintf(cmd, "AT+CIPSTART=\"%s\",\"%s\",%d\r\n", mode, ip, port);
    ESP8266_Status status = ESP8266_SendATCommand(cmd, "CONNECT", 10000);
    
    if(status != ESP8266_OK)
    {
        extern u8 USART3_RX_BUF[];
        extern u16 USART3_RX_STA;
        if(strstr((char*)USART3_RX_BUF, "ALREADY CONNECTED") != NULL)
        {
            printf("[ESP8266] Server already connected\r\n");
            return ESP8266_OK;
        }
        printf("[ESP8266] Server connection failed\r\n");
    }
    else
    {
        printf("[ESP8266] Server connected successfully\r\n");
    }
    return status;
}

ESP8266_Status ESP8266_CloseConnection(void)
{
    return ESP8266_SendATCommand("AT+CIPCLOSE\r\n", "OK", 2000);
}

ESP8266_Status ESP8266_SetMuxMode(u8 mode)
{
    char cmd[32];
    sprintf(cmd, "AT+CIPMUX=%d\r\n", mode);
    printf("[ESP8266] Setting MUX mode to %d...\r\n", mode);
    return ESP8266_SendATCommand(cmd, "OK", 2000);
}

/**
 * @brief  进入硬件透传链路层
 */
ESP8266_Status ESP8266_EnterTransparentMode(void)
{
    printf("[ESP8266] Entering transparent mode...\r\n");
    
    if(ESP8266_SetMuxMode(0) != ESP8266_OK)
    {
        printf("[ESP8266] Failed to set single connection mode\r\n");
        return ESP8266_ERROR;
    }
    delay_ms(500);
    
    if(ESP8266_SendATCommand("AT+CIPMODE=1\r\n", "OK", 1000) != ESP8266_OK)
    {
        printf("[ESP8266] Failed to set transparent mode\r\n");
        return ESP8266_ERROR;
    }
    delay_ms(500);
    
    if(ESP8266_SendATCommand("AT+CIPSEND\r\n", ">", 1000) != ESP8266_OK)
    {
        printf("[ESP8266] Failed to start transparent transmission\r\n");
        return ESP8266_ERROR;
    }
    
    printf("[ESP8266] Transparent mode enabled\r\n");
    return ESP8266_OK;
}

/**
 * @brief  断开透传链路
 */
ESP8266_Status ESP8266_ExitTransparentMode(void)
{
    printf("[ESP8266] Exiting transparent mode...\r\n");
    USART3_Send_String("+++");
    delay_ms(1000);
    return ESP8266_OK;
}

/**
 * @brief  通过 AT 指令发送定长数据
 */
ESP8266_Status ESP8266_SendData(char* data, u16 len)
{
    char cmd[32];
    sprintf(cmd, "AT+CIPSEND=0,%d\r\n", len);
    
    if(ESP8266_SendATCommand(cmd, ">", 1000) != ESP8266_OK)
    {
        return ESP8266_ERROR;
    }
    USART3_Send_Data((u8*)data, len);
    delay_ms(100);
    return ESP8266_OK;
}

ESP8266_Status ESP8266_GetIP(void)
{
    printf("[ESP8266] Getting IP address...\r\n");
    return ESP8266_SendATCommand("AT+CIFSR\r\n", "OK", 2000);
}

ESP8266_Status ESP8266_GetMAC(void)
{
    printf("[ESP8266] Getting MAC address...\r\n");
    return ESP8266_SendATCommand("AT+CIPSTAMAC?\r\n", "OK", 2000);
}
