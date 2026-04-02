#ifndef __ETHERNET_REPORT_H__
#define __ETHERNET_REPORT_H__

#include "stm32f4xx.h"  // 根据你的实际单片机型号修改，比如 stm32f10x.h
#include "FreeRTOS.h"
#include "task.h"

// 备用网口链路的目标服务器配置（这里假设发给你的电脑调试）
#define BACKUP_SERVER_IP    "192.168.0.101" // 你的电脑 IP
#define BACKUP_SERVER_PORT  8080            // 接收端口

// 函数声明
void Ethernet_Report_SendSensorData(void);

#endif /* __ETHERNET_REPORT_H__ */
