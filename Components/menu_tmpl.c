#include "stddef.h"
#include <menu_tmpl.h>
#include <OLED_service.h>
#include <USART_task.h>

static menuItem_t *sp_currentItem = NULL;
static menuItem_t *sp_currentPage = NULL;
static menuItem_t *sp_executingItem = NULL;

void menu_node_init(menuItem_t *p_rootItem) {
	if (p_rootItem == NULL) {
		sp_currentItem = NULL;
		sp_currentPage = NULL;
		return;
	}
	sp_currentPage = p_rootItem;
	sp_currentItem = p_rootItem->p_child;
}

static menuItem_t *menu_get_last_child(menuItem_t *page) {
	menuItem_t *item = page->p_child;

	if (item == NULL) {
			return NULL;
	}

	while (item->p_next != NULL) {
			item = item->p_next;
	}

	return item;
}

void menu_navigate(e_menuKey_t e_key) {
	if (sp_currentPage == NULL) {
		return;
  }
	
  if (sp_currentItem == NULL) {
		sp_currentItem = sp_currentPage->p_child;
		// ҳ����ʱǿ���˻ظ���
		if (sp_currentItem == NULL && sp_currentPage->p_parent != NULL) {
			sp_currentItem = sp_currentPage;
			sp_currentPage = sp_currentPage->p_parent;
		}
		// �����Ȼ��Ч���˳�
		if (sp_currentItem == NULL) {
			return;
		}
	}
  
	USART_printf(
		"key=%d page=%s item=%s\r\n", e_key,
		sp_currentPage ? sp_currentPage->p_name : "NULL",
		sp_currentItem ? sp_currentItem->p_name : "NULL"
  );
	switch (e_key) {
		case MENU_KEY_UP:
			if (sp_currentItem == NULL) {
          break;
      }
			if (sp_currentItem->p_prev != NULL) {
				sp_currentItem = sp_currentItem->p_prev;
			}
			else {
				menuItem_t *lastItem = menu_get_last_child(sp_currentPage);
				if (lastItem != NULL) {
					sp_currentItem = lastItem;
				}
			}
			USART_printf("UP\r\n");
			break;

		case MENU_KEY_DOWN:
			if (sp_currentItem == NULL) {
          break;
      }
			if (sp_currentItem->p_next != NULL) {
				sp_currentItem = sp_currentItem->p_next;
			}
			else {
				menuItem_t *firstItem = sp_currentPage->p_child;
				if (firstItem != NULL) {
					sp_currentItem = firstItem;
				}
			}
			USART_printf("DOWN\r\n");
			break;

		case MENU_KEY_ENTER:
			if ((sp_currentItem->e_type == MENU_ITEM_TYPE_SUBMENU) 
					&& (sp_currentItem->p_child != NULL)) {
				sp_currentPage = sp_currentItem;
				sp_currentItem = sp_currentItem->p_child;
				if (sp_currentPage->fn_enter_callback != NULL) {
					sp_currentPage->fn_enter_callback();
				}
				USART_printf("ENTER\r\n");
			}
			else if (sp_currentItem->e_type == MENU_ITEM_TYPE_ACTION) {
				if (sp_currentItem->fn_action_callback != NULL) {
					sp_currentItem->fn_action_callback();
					sp_executingItem = NULL;
					USART_printf("ACTION\r\n");
					}
			}
			else if (sp_currentItem->e_type == MENU_ITEM_TYPE_VALUE) {
				if (sp_currentItem->fn_value_adjust_callback != NULL) {
					sp_currentItem->fn_value_adjust_callback(*sp_currentItem->p_value);
				}
				USART_printf("VALUE\r\n");
			}
			break;

		case MENU_KEY_BACK:
			if ((sp_currentPage != NULL) && (sp_currentPage->p_parent != NULL)) {
				sp_currentItem = sp_currentPage;
				sp_currentPage = sp_currentPage->p_parent;
				USART_printf("BACK\r\n");
			}
			break;
		default:
			return;
	}

	OLED_proc();
}

const menuItem_t *menu_get_current_item(void) {
	return sp_currentItem;
}

const menuItem_t *menu_get_current_page(void) {
	return sp_currentPage;
}

void menu_create_submenu(menuItem_t *p_item,
	const char *p_name,
	uint8_t     page_ID,
	menuItem_t *p_next,
	menuItem_t *p_prev,
	menuItem_t *p_parent,
	menuItem_t *p_child) {
		p_item->p_name                    = p_name;
		p_item->page_ID                   = page_ID;
		p_item->e_type                    = MENU_ITEM_TYPE_SUBMENU;
		p_item->p_next                    = p_next;
		p_item->p_prev                    = p_prev;
		p_item->p_parent                  = p_parent;
		p_item->p_child                   = p_child;
		p_item->fn_enter_callback         = NULL;
		p_item->fn_action_callback        = NULL;
		p_item->fn_value_adjust_callback  = NULL;
		p_item->fn_toggle_callback        = NULL;
		p_item->p_value                   = NULL;
		p_item->valueMin                  = 0;
		p_item->valueMax                  = 0;
		p_item->p_toggle                  = NULL;
		p_item->context                   = 0;
}

void menu_bind_enter(menuItem_t *p_item,
	void (*fn_enter_callback)(void)) {
		p_item->fn_enter_callback       = fn_enter_callback;
		p_item->e_type                  = MENU_ITEM_TYPE_SUBMENU;
}

void menu_bind_action(menuItem_t *p_item,
	void (*fn_action_callback)(void)) {
		p_item->fn_action_callback       = fn_action_callback;
		p_item->e_type                   = MENU_ITEM_TYPE_ACTION;
}

void menu_bind_value(menuItem_t *p_item,
	int16_t *p_value,
	int16_t valueMin,
	int16_t valueMax,
	void (*fn_value_adjust_callback)(int16_t)) {
		p_item->fn_value_adjust_callback = fn_value_adjust_callback;
		p_item->p_value                  = p_value;
		p_item->valueMin                 = valueMin;
		p_item->valueMax                 = valueMax;
		p_item->e_type                   = MENU_ITEM_TYPE_VALUE;
}

void menu_bind_toggle(menuItem_t *p_item,
	bool *p_toggle,
	void (*fn_toggle_callback)(void)) {
		p_item->fn_toggle_callback       = fn_toggle_callback;
		p_item->p_toggle                 = p_toggle;
		p_item->e_type                   = MENU_ITEM_TYPE_TOGGLE;
}

void menu_set_context(menuItem_t *p_item, uint32_t context) {
	if (p_item != NULL) {
			p_item->context = context;
	}
}

uint32_t menu_get_current_context(void) {
	if (sp_executingItem != NULL) {
			return sp_executingItem->context;
	}
	return 0;
}
