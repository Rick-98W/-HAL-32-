#include "stddef.h"

#define IS_MENU_CONSTR
#include <menu_item.h>
#include <LED_task.h> 
#include <OLED_service.h>
#include <OLED_page.h>

static bool isInLEDToggle = false;
//菜单元节点与一级界面
static menuItem_t s_rootItem;
static menuItem_t s_mainLEDItem;
static menuItem_t s_mainADCItem;
static menuItem_t s_mainSystemItem;
//菜单二级界面
//LED菜单
static menuItem_t s_LEDSaveState;
static menuItem_t s_LEDLoadState;
static menuItem_t s_LEDBlackOut;
static menuItem_t s_LEDLightUp;
static menuItem_t s_LED_toggle_phase;

void LED_action_blackout(void) {LED_set_blackout(1);}
void LED_action_lightup(void) {LED_set_blackout(0);}

const bool menu_is_in_LED_toggle(void) {
	return isInLEDToggle;
}

void LED_toggle_set_flag(void) {
	isInLEDToggle = true;
}

void menu_LED_toggle_exit(void) {
	isInLEDToggle = false;
	OLED_proc();
}

void menu_LED_toggle_phase_wrapper(uint8_t LEDIndex) {
	LED_toggle_phase(LEDIndex);
	OLED_page_set_toggle_feedback(LEDIndex);
	OLED_proc();
}

void menu_init(void) {
	//												NODE					 NODE NAME			PAGE ID	        NEXT					  		    PREV                  PARENT              CHILD
	menu_create_submenu(&s_rootItem,         "Main",      MAIN_PAGE,    NULL,                  NULL,                 NULL,                 &s_mainLEDItem);

	menu_create_submenu(&s_mainLEDItem,      "LED",       LED_PAGE,     &s_mainADCItem,        NULL,                 &s_rootItem,          &s_LEDSaveState);
	menu_create_submenu(&s_mainADCItem,      "ADC",       ADC_PAGE,     &s_mainSystemItem,     &s_mainLEDItem,       &s_rootItem,          NULL);
	menu_create_submenu(&s_mainSystemItem,   "System",    SYSTEM_PAGE,  NULL,                  &s_mainADCItem,       &s_rootItem,          NULL);
	
	menu_create_submenu(&s_LEDSaveState,     "LEDSave",      NULL,      &s_LEDLoadState,       NULL,                 &s_mainLEDItem,       NULL);
	menu_bind_action(&s_LEDSaveState, LED_save_state);	
	
	menu_create_submenu(&s_LEDLoadState,     "LEDLoad",      NULL,      &s_LEDBlackOut,        &s_LEDSaveState,      &s_mainLEDItem,       NULL);
	menu_bind_action(&s_LEDLoadState, LED_restore_state);

	
	menu_create_submenu(&s_LEDBlackOut,      "LEDBlackOut",  NULL,      &s_LEDLightUp,         &s_LEDLoadState,      &s_mainLEDItem,       NULL);
	menu_bind_action(&s_LEDBlackOut, LED_action_blackout);

	
	menu_create_submenu(&s_LEDLightUp,       "LEDLightUp",   NULL,      &s_LED_toggle_phase,   &s_LEDBlackOut,       &s_mainLEDItem,       NULL);
	menu_bind_action(&s_LEDLightUp, LED_action_lightup);
	
	menu_create_submenu(&s_LED_toggle_phase, "LEDToggle",   LEDToggle,  NULL,                  &s_LEDLightUp,        &s_mainLEDItem,       NULL);
	menu_bind_action(&s_LED_toggle_phase, LED_toggle_set_flag);

	menu_node_init(&s_rootItem);
}

