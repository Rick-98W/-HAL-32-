#ifndef MENU_LED_SUPPL_H
#define MENU_LED_SUPPL_H

#include <menu_item.h>

/* L6 App: LED submenu construction and actions. */
void menu_LED_suppl_init(menuItem_t *parent, menu_mode_t *mode);
void menu_LED_suppl_handle_key(menu_input_t input);

#endif
