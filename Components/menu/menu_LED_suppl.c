#include <stddef.h>

#include <menu_tmpl.h>
#include <menu_LED_suppl.h>
#include <LED_task.h>
#include <OLED_service.h>
#include <OLED_page.h>

static menuItem_t s_LEDSaveState;
static menuItem_t s_LEDLoadState;
static menuItem_t s_LEDBlackOut;
static menuItem_t s_LEDLightUp;
static menuItem_t s_LED_toggle_phase;

static void LED_action_blackout(void) {LED_set_blackout(1);}
static void LED_action_lightup(void) {LED_set_blackout(0);}

static void LED_toggle_enter(void) {
	menu_LED_toggle_enter();
}

void menu_LED_suppl_init(menuItem_t *parent) {
	//												NODE							NAME				PAGEID			NEXT								PREV							PARENT	CHILD
	menu_create_submenu(&s_LEDSaveState,     "LEDSave",     NULL_PAGE, &s_LEDLoadState,     NULL,             parent, NULL);
	menu_bind_action(&s_LEDSaveState, LED_save_state);
	menu_create_submenu(&s_LEDLoadState,     "LEDLoad",     NULL_PAGE, &s_LEDBlackOut,      &s_LEDSaveState,  parent, NULL);
	menu_bind_action(&s_LEDLoadState, LED_restore_state);
	menu_create_submenu(&s_LEDBlackOut,      "LEDBlackOut", NULL_PAGE, &s_LEDLightUp,       &s_LEDLoadState,  parent, NULL);
	menu_bind_action(&s_LEDBlackOut, LED_action_blackout);
	menu_create_submenu(&s_LEDLightUp,       "LEDLightUp",  NULL_PAGE, &s_LED_toggle_phase, &s_LEDBlackOut,   parent, NULL);
	menu_bind_action(&s_LEDLightUp, LED_action_lightup);
	menu_create_submenu(&s_LED_toggle_phase, "LEDToggle",   LEDToggle, NULL,                &s_LEDLightUp,    parent, NULL);
	menu_bind_action(&s_LED_toggle_phase, LED_toggle_enter);
	parent->p_child = &s_LEDSaveState;
}

void menu_LED_suppl_handle_key(menu_input_t input) {
	LED_toggle_phase(input);
	OLED_page_set_toggle_feedback(input);
	OLED_proc();
}
