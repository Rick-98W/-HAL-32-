#ifndef MENU_TMPL_H
#define MENU_TMPL_H

/* Menu-family internal. Other modules include menu_item.h only. */

#include "stdint.h"
#include "stdbool.h"
#include <menu_item.h>

typedef enum {
	MENU_ITEM_TYPE_SUBMENU,
	MENU_ITEM_TYPE_ACTION,
	MENU_ITEM_TYPE_VALUE,
	MENU_ITEM_TYPE_TOGGLE
} e_menuItemType_t;

typedef enum {
	MENU_KEY_UP,
	MENU_KEY_DOWN,
	MENU_KEY_ENTER,
	MENU_KEY_BACK
} e_menuKey_t;

struct menuItem_t {
	const char *p_name;
	uint8_t page_ID;
	e_menuItemType_t e_type;

	struct menuItem_t *p_next;
	struct menuItem_t *p_prev;
	struct menuItem_t *p_parent;
	struct menuItem_t *p_child;

	void (*fn_enter_callback)(void);
	void (*fn_action_callback)(void);
	void (*fn_value_enter_callback)(const menuItem_t *);
	void (*fn_value_adjust_callback)(int16_t);
	void (*fn_toggle_callback)(void);

	int16_t  *p_value;
	int16_t  valueMin;
	int16_t  valueMax;

	bool  *p_toggle;

	uint32_t context;
};

void menu_create_submenu(menuItem_t *p_item,
	const char *p_name,
	uint8_t     page_ID,
	menuItem_t *p_next,
	menuItem_t *p_prev,
	menuItem_t *p_parent,
	menuItem_t *p_child);

void menu_bind_enter(menuItem_t *p_item,
	void (*fn_enter_callback)(void));

void menu_bind_action(menuItem_t *p_item,
	void (*fn_action_callback)(void));

void menu_bind_value(menuItem_t *p_item,
	int16_t *p_value,
	int16_t valueMin,
	int16_t valueMax,
	void (*fn_value_enter_callback)(const menuItem_t *),
	void (*fn_value_adjust_callback)(int16_t));

void menu_bind_toggle(menuItem_t *p_item,
	bool *p_toggle,
	void (*fn_toggle_callback)(void));

void menu_set_context(menuItem_t *p_item, uint32_t context);

/* Put the highlighted item onto p_item, which must be a child of the
   current page. Used by an enter callback to open a page with the cursor
   already sitting on the relevant child. */
void menu_focus_item(menuItem_t *p_item);

void menu_node_init(menuItem_t *rootItem);
void menu_navigate(e_menuKey_t e_Key);
uint32_t menu_get_current_context(void);

#endif
