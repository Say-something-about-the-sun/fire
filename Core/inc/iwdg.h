/**
 * @file    iwdg.h
 * @brief   独立看门狗驱动接口
 */
#ifndef __IWDG_H
#define __IWDG_H
#include "sys.h"

void IWDG_Init(u8 prer, u16 rlr);
void IWDG_Feed(void);

#endif /* __IWDG_H */
