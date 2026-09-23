#ifndef OLED_TASK_H
#define OLED_TASK_H

#include "main.h"

void OLED_init(void);
void OLED_proc(void);
void OLED_printf(uint8_t x, uint8_t y, uint8_t fontsize, const char *fmt, ...);

void WouoUI_init();
	
#endif