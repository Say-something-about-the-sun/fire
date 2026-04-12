// task_network.h
#ifndef __TASK_NETWORK_H
#define __TASK_NETWORK_H

#include "sys.h"

// 仅暴露初始化接口
// main.c 只需要调用这个函数，网络任务就会自己在后台跑起来
void Network_Task_Init(void);

#endif
