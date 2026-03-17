#ifndef __ESP8266_H
#define __ESP8266_H

#include "sys.h"

// ESP8266响应超时时间
#define ESP8266_TIMEOUT_MS  5000

// ESP8266状态码
typedef enum {
    ESP8266_OK = 0,
    ESP8266_ERROR = 1,
    ESP8266_TIMEOUT = 2,
    ESP8266_NO_RESPONSE = 3
} ESP8266_Status;

// 函数声明
void ESP8266_Init(void);
ESP8266_Status ESP8266_SendATCommand(char* cmd, char* expected_response, u32 timeout_ms);
ESP8266_Status ESP8266_CheckStatus(void);
ESP8266_Status ESP8266_SetMode(u8 mode);
ESP8266_Status ESP8266_ConnectWiFi(char* ssid, char* password);
ESP8266_Status ESP8266_DisconnectWiFi(void);
ESP8266_Status ESP8266_EnableDHCP(void);
ESP8266_Status ESP8266_SetMuxMode(u8 mode);
ESP8266_Status ESP8266_ConnectServer(char* mode, char* ip, u16 port);
ESP8266_Status ESP8266_CloseConnection(void);
ESP8266_Status ESP8266_EnterTransparentMode(void);
ESP8266_Status ESP8266_ExitTransparentMode(void);
ESP8266_Status ESP8266_SendData(char* data, u16 len);
ESP8266_Status ESP8266_GetIP(void);
ESP8266_Status ESP8266_GetMAC(void);

#endif
