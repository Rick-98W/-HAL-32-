#include <stddef.h>

#define IS_MENU_CONSTR
#include <menu_ADC_suppl.h>

static menu_mode_t *s_menu_mode;
static menuItem_t s_DACSetFSR;
static menuItem_t s_DACSetPeriod;
static int16_t s_DACFSR = 3300;

static void DACFSR_enter_adjust(int16_t value) {
	(void)value;
	*s_menu_mode = MENU_MODE_VALUE_ADJUST;
}

void menu_ADC_suppl_init(menuItem_t *parent, menu_mode_t *mode) {
	s_menu_mode = mode;
	menu_create_submenu(&s_DACSetFSR,    "DACSetFSR",    DACSetFSR,    &s_DACSetPeriod, NULL,         parent, NULL);
	menu_bind_value(&s_DACSetFSR, &s_DACFSR, 0, 3300, DACFSR_enter_adjust);
	menu_create_submenu(&s_DACSetPeriod, "DACSetPeriod", DACSetPeriod, NULL,            &s_DACSetFSR, parent, NULL);
	parent->p_child = &s_DACSetFSR;
}
