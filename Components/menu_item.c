#include "stddef.h"

#define IS_MENU_CONSTR
#include <menu_item.h>
#include <LED_task.h> 
#include <OLED_service.h>
#include <OLED_page.h>

static menu_mode_t s_menu_mode = MENU_MODE_NORMAL;
//菜单元节点与一级界面
static menuItem_t s_rootItem;
static menuItem_t s_mainLEDItem;
static menuItem_t s_mainADCItem;
static menuItem_t s_mainSystemItem;
//测试用日志显示
static menuItem_t s_logItem;
//菜单二级界面
//LED菜单
static menuItem_t s_LEDSaveState;
static menuItem_t s_LEDLoadState;
static menuItem_t s_LEDBlackOut;
static menuItem_t s_LEDLightUp;
static menuItem_t s_LED_toggle_phase;
//ADC菜单
static menuItem_t s_DACSetFSR;
static menuItem_t s_DACSetPeriod;

const menu_mode_t menu_get_mode(void) {
	return s_menu_mode;
}

static void menu_return_normal_mode(void) {
	s_menu_mode = MENU_MODE_NORMAL;
	OLED_proc();
}

static void menu_log_set_flag(void) {
	s_menu_mode = MENU_MODE_LOG;
}

//LED相关补充业务
static void LED_action_blackout(void) {LED_set_blackout(1);}
static void LED_action_lightup(void) {LED_set_blackout(0);}

static void LED_toggle_set_flag(void) {
	s_menu_mode = MENU_MODE_LED_TOGGLE;
}

static void menu_LED_toggle_phase_wrapper(uint8_t LEDIndex) {
	LED_toggle_phase(LEDIndex);
	OLED_page_set_toggle_feedback(LEDIndex);
	OLED_proc();
}

//ADC相关补充业务
#define VALUE_ARM_THRESHOLD 14

static const menuItem_t *s_editItem = NULL;
static int16_t s_editValue = 0;
static int32_t s_editRawAtEntry = 0;
static int32_t s_lastRaw = 0;
static bool s_editArmed = false;

static int32_t value_map(int32_t raw, int16_t vmin, int16_t vmax) {
    return vmin + (raw * (vmax - vmin) + 2047) / 4095;
}

static void value_edit_begin(const menuItem_t *p_item) {
    s_editItem = p_item;
    s_editValue = *p_item->p_value;
    s_editRawAtEntry = s_lastRaw;
    s_editArmed = false;
    s_menu_mode = MENU_MODE_VALUE_EDIT;
    OLED_proc();
}

static void value_edit_exit(void) {
    s_editItem = NULL;
    s_menu_mode = MENU_MODE_NORMAL;
    OLED_proc();
}

void menu_init(void) {
	//												NODE					 NODE NAME		    	PAGE ID	           NEXT					  		    PREV                PARENT              CHILD
	menu_create_submenu(&s_rootItem,         "Main",          MAIN_PAGE,      NULL,                  NULL,                 NULL,                 &s_mainLEDItem);

	menu_create_submenu(&s_mainLEDItem,      "LED",           LED_PAGE,       &s_mainADCItem,        NULL,                 &s_rootItem,          &s_LEDSaveState);
	menu_create_submenu(&s_mainADCItem,      "ADC",           ADC_PAGE,       &s_mainSystemItem,     &s_mainLEDItem,       &s_rootItem,          &s_DACSetFSR);
	menu_create_submenu(&s_mainSystemItem,   "System",        SYSTEM_PAGE,    &s_logItem,            &s_mainADCItem,       &s_rootItem,          NULL);
	menu_create_submenu(&s_logItem,  				 "Log",           LOG_PAGE, 	    NULL,                  &s_mainSystemItem,    &s_rootItem,          NULL);
	menu_bind_action(&s_logItem, menu_log_set_flag);
	
	menu_create_submenu(&s_LEDSaveState,     "LEDSave",          NULL,        &s_LEDLoadState,       NULL,                 &s_mainLEDItem,       NULL);
	menu_bind_action(&s_LEDSaveState, LED_save_state);	
	menu_create_submenu(&s_LEDLoadState,     "LEDLoad",          NULL,        &s_LEDBlackOut,        &s_LEDSaveState,      &s_mainLEDItem,       NULL);
	menu_bind_action(&s_LEDLoadState, LED_restore_state);
	menu_create_submenu(&s_LEDBlackOut,      "LEDBlackOut",      NULL,        &s_LEDLightUp,         &s_LEDLoadState,      &s_mainLEDItem,       NULL);
	menu_bind_action(&s_LEDBlackOut, LED_action_blackout);
	menu_create_submenu(&s_LEDLightUp,       "LEDLightUp",       NULL,        &s_LED_toggle_phase,   &s_LEDBlackOut,       &s_mainLEDItem,       NULL);
	menu_bind_action(&s_LEDLightUp, LED_action_lightup);
	menu_create_submenu(&s_LED_toggle_phase, "LEDToggle",     LEDToggle,      NULL,                  &s_LEDLightUp,        &s_mainLEDItem,       NULL);
	menu_bind_action(&s_LED_toggle_phase, LED_toggle_set_flag);

	menu_create_submenu(&s_DACSetFSR,  		   "DACSetFSR",     DACSetFSR,      &s_DACSetPeriod,       NULL,                 &s_mainADCItem,       NULL);
	menu_create_submenu(&s_DACSetPeriod,     "DACSetPeriod",  DACSetPeriod,   NULL,                  &s_DACSetFSR,         &s_mainADCItem,       NULL);
	
	menu_node_init(&s_rootItem);
}

void menu_handle_key(menu_input_t input) {
	switch (s_menu_mode) {
		case MENU_MODE_LED_TOGGLE:
			if (input <= MENU_INPUT_6) {
					menu_LED_toggle_phase_wrapper(input);
			} else if (input == MENU_INPUT_WKUP) {
					menu_return_normal_mode();
			}
			break;

		case MENU_MODE_LOG:
			if (input == MENU_INPUT_4) {
					menu_return_normal_mode();
			}
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