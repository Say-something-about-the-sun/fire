/**
 * @file    task_hmi.h
 * @brief   人机交互界面任务头文件
 * @note    提供本地按键响应及系统状态干预接口。
 */
#ifndef __TASK_HMI_H
#define __TASK_HMI_H

#include "sys.h"

/* 仅向系统核心开放任务初始化接口 */
void HMI_Task_Init(void);

#endif /* __TASK_HMI_H */
