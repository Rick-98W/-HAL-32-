#ifndef OLED_PAGE_H
#define OLED_PAGE_H

#include <stdint.h>

void OLED_page_init(void);
void OLED_page_render(void);
void OLED_page_set_toggle_feedback(uint8_t LEDIndex);
	
#endif