#ifndef __LED_H
#define __LED_H

#include "stm32f4xx_gpio.h"
#include "string.h"



#define led0 "led0"
#define led1 "led1"
#define led2 "led2"

void LED_Init(void);
void led_on(char *ch);
void led_off(char *ch);



#endif
