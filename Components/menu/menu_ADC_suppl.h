#ifndef MENU_ADC_SUPPL_H
#define MENU_ADC_SUPPL_H

#include <menu_item.h>

/* L6 App: ADC submenu construction. */
void menu_ADC_suppl_init(menuItem_t *parent);
void menu_set_ADC_value(const menuItem_t *item);
void DAC_menu_refresh_FSR(void);
void DAC_menu_refresh_period(void);
void DAC_menu_refresh_waveType(void);
void ADC_menu_refresh_wave(void);
void ADC_menu_refresh_ratio(void);

#endif
