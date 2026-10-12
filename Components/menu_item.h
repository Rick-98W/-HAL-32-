#ifndef MENU_ITEM_H
#define MENU_ITEM_H

#include <menu_tmpl.h>

enum {
	MAIN_PAGE,
	LED_PAGE,
	ADC_PAGE,
	SYSTEM_PAGE,
	LOG_PAGE,
	LEDToggle,
	DACSetFSR,
	DACSetPeriod,
};

typedef enum {
	MENU_MODE_NORMAL,
	MENU_MODE_LED_TOGGLE,
	MENU_MODE_LOG,
	MENU_MODE_VALUE_EDIT,
} menu_mode_t;

typedef enum {
	MENU_INPUT_1 = 0,
	MENU_INPUT_2,
	MENU_INPUT_3,
	MENU_INPUT_4,
	MENU_INPUT_5,
	MENU_INPUT_6,
	MENU_INPUT_WKUP,
} menu_input_t;

void menu_init(void);
const menu_mode_t menu_get_mode(void);
void menu_handle_key(menu_input_t input);

void menu_on_knob_raw(int32_t raw);
const menuItem_t *menu_get_edit_item(void);
int16_t menu_get_edit_value(void);

#endif