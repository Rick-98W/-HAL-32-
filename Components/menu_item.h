#ifndef MENU_ITEM_H
#define MENU_ITEM_H

#include <menu_tmpl.h>

enum {
	MAIN_PAGE,
	LED_PAGE,
	ADC_PAGE,
	SYSTEM_PAGE,
	LEDToggle,
};


void menu_init(void);
const bool menu_is_in_LED_toggle(void);
void menu_LED_toggle_exit(void);
void menu_LED_toggle_phase_wrapper(uint8_t LEDIndex);

#endif