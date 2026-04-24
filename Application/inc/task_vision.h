/**
 * @file    task_vision.h
 * @brief   图像捕捉管道与视觉算法调度任务头文件
 */
#ifndef __TASK_VISION_H
#define __TASK_VISION_H

#include "sys.h"
#include "FreeRTOS.h"  
#include "task.h"      

extern TaskHandle_t VisionTask_Handler;

void Vision_Task_Init(void);

#endif /* __TASK_VISION_H */
