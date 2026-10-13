#include <stddef.h>

#define IS_MENU_CONSTR
#include <menu_item.h>
#include <menu_ADC_suppl.h>
#include <menu_LED_suppl.h>
#include <OLED_service.h>
#include <knob_task.h>

static menu_mode_t s_menu_mode = MENU_MODE_NORMAL;
static menuItem_t s_rootItem;
static menuItem_t s_mainLEDItem;
static menuItem_t s_mainADCItem;
static menuItem_t s_mainSystemItem;
static menuItem_t s_logItem;

static int16_t s_editValue = 0;
static bool s_valueEditing = false;

menu_mode_t menu_get_mode(void) {
	return s_menu_mode;
}

static void menu_return_normal_mode(void) {
	s_menu_mode = MENU_MODE_NORMAL;
	OLED_proc();
}

static void menu_log_set_flag(void) {
	s_menu_mode = MENU_MODE_LOG;
}

static int16_t value_map(uint32_t raw, int16_t valMin, int16_t valMax) {
	return (int16_t)(valMin + (int32_t)((raw * (uint32_t)(valMax - valMin) + 2047) / 4095));
}

static void menu_on_knob_change(uint32_t raw) {
	if (s_menu_mode != MENU_MODE_VALUE_ADJUST || !s_valueEditing) {
		return;
	}
	const menuItem_t *item = menu_get_current_item();
	int16_t v = value_map(raw, item->valueMin, item->valueMax);
	if (v != s_editValue) {
		s_editValue = v;
		OLED_proc();
	}
}

void menu_init(void) {
	menu_create_submenu(&s_rootItem,       "Main",   MAIN_PAGE,   NULL,              NULL,              NULL,        &s_mainLEDItem);
	menu_create_submenu(&s_mainLEDItem,    "LED",    LED_PAGE,    &s_mainADCItem,    NULL,              &s_rootItem, NULL);
	menu_create_submenu(&s_mainADCItem,    "ADC",    ADC_PAGE,    &s_mainSystemItem, &s_mainLEDItem,    &s_rootItem, NULL);
	menu_create_submenu(&s_mainSystemItem, "System", SYSTEM_PAGE, &s_logItem,        &s_mainADCItem,    &s_rootItem, NULL);
	menu_create_submenu(&s_logItem,        "Log",    LOG_PAGE,    NULL,              &s_mainSystemItem, &s_rootItem, NULL);
	menu_bind_action(&s_logItem, menu_log_set_flag);

	menu_LED_suppl_init(&s_mainLEDItem, &s_menu_mode);
	menu_ADC_suppl_init(&s_mainADCItem, &s_menu_mode);
	menu_node_init(&s_rootItem);
	knob_register_change_cb(menu_on_knob_change);
}

bool menu_value_is_editing(void) {
	return s_valueEditing;
}

int16_t menu_get_edit_value(void) {
	return s_editValue;
}

static void menu_value_route(menu_input_t input) {
	if (input != MENU_INPUT_3 && input != MENU_INPUT_4) {
		return;
	}
	const menuItem_t *item = menu_get_current_item();

	if (input == MENU_INPUT_3 && !s_valueEditing) {
		s_editValue = value_map(knob_get_value(), item->valueMin, item->valueMax);
		s_valueEditing = true;
	}
	else if (input == MENU_INPUT_3) {
		*item->p_value = s_editValue;
		s_valueEditing = false;
	}
	else if (input == MENU_INPUT_4 && s_valueEditing) {
		s_valueEditing = false;
	}
	else if (input == MENU_INPUT_4) {
		s_menu_mode = MENU_MODE_NORMAL;
	}
	OLED_proc();
}

void menu_handle_key(menu_input_t input) {
	switch (s_menu_mode) {
		case MENU_MODE_LED_TOGGLE:
			menu_LED_suppl_handle_key(input);
			break;

		case MENU_MODE_LOG:
			if (input == MENU_INPUT_4) {
				menu_return_normal_mode();
			}
			break;

		case MENU_MODE_VALUE_ADJUST:
			menu_value_route(input);
			break;

		case MENU_MODE_NORMAL:
		default:
			switch (input) {
				case MENU_INPUT_1: menu_navigate(MENU_KEY_UP);    break;
				case MENU_INPUT_2: menu_navigate(MENU_KEY_DOWN);  break;
				case MENU_INPUT_3: menu_navigate(MENU_KEY_ENTER); break;
				case MENU_INPUT_4: menu_navigate(MENU_KEY_BACK);  break;
				default: break;
			}
			break;
	}
}
