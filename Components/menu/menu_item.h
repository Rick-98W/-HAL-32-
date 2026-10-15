#ifndef MENU_ITEM_H
#define MENU_ITEM_H

#include <stdint.h>
#include <stdbool.h>

typedef struct menuItem_t menuItem_t;

enum {
	NULL_PAGE,
	MAIN_PAGE,
	LED_PAGE,
	ADC_PAGE,
	SYSTEM_PAGE,
	LOG_PAGE,
	LEDToggle,
	DACSetFSR,
	DACSetPeriod,
	DACSetWave,
};

typedef enum {
	MENU_MODE_NORMAL,
	MENU_MODE_LED_TOGGLE,
	MENU_MODE_LOG,
	MENU_MODE_VALUE_ADJUST,
} menu_mode_t;

typedef enum {
	MENU_INPUT_1,
	MENU_INPUT_2,
	MENU_INPUT_3,
	MENU_INPUT_4,
	MENU_INPUT_5,
	MENU_INPUT_6,
	MENU_INPUT_WKUP,
} menu_input_t;

void menu_init(void);
menu_mode_t menu_get_mode(void);
void menu_handle_key(menu_input_t input);
void menu_value_enter(const menuItem_t *item);
void menu_led_toggle_enter(void);

bool menu_value_is_editing(void);
int16_t menu_get_edit_value(void);

const menuItem_t *menu_get_current_page(void);
const menuItem_t *menu_get_current_item(void);
const char *menu_item_name(const menuItem_t *item);
const menuItem_t *menu_item_child(const menuItem_t *item);
const menuItem_t *menu_item_next(const menuItem_t *item);
int16_t menu_item_value(const menuItem_t *item);

#endif
