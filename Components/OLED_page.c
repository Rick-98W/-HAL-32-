#include <OLED_page.h>
#include <menu_item.h>
#include <OLED_service.h>
#include "main.h"
#include "u8g2.h"

#define OLED_VISIBLE_ROWS 3

static uint8_t s_feedbackLED;
static bool s_feedbackValid = false;
	
const static menuItem_t *sp_currentItem = NULL;
const static menuItem_t *sp_currentPage = NULL;

static void OLED_LED_toggle_page_render(u8g2_t *u8g2) {
	u8g2_DrawStr(u8g2, 0, 8, "LED Toggling...");
	if (s_feedbackValid) {
		static const char *feedback[] = {
				"LED1 toggled",
				"LED2 toggled",
				"LED3 toggled",
				"LED4 toggled",
				"LED5 toggled",
				"LED6 toggled"
		};
		u8g2_DrawStr(u8g2, 0, 20, feedback[s_feedbackLED]);
		s_feedbackValid = false;
		OLED_request_refresh(2000);
	}
	u8g2_DrawStr(u8g2, 0, 32, "Press WKUP to exit.");
}

static void OLED_LOG_page_render(u8g2_t *u8g2) {
	u8g2_DrawStr(u8g2, 0, 8, "LOG Presenting...");
}

static void OLED_draw_items(u8g2_t *u8g2, const menuItem_t *page, const menuItem_t *selected) {
	if (selected == NULL) {
      return;
  }
	const menuItem_t *item;
	uint8_t count = 0;
	uint8_t selected_index = 0;
	uint8_t start_index;
	uint8_t row;

	for (item = page->p_child; item != NULL; item = item->p_next) {
			if (item == selected) {
					selected_index = count;
			}
			count++;
	}

	if (count == 0) {
			return;
	}

	start_index = selected_index > 0 ? selected_index - 1 : 0;

	if (start_index + OLED_VISIBLE_ROWS > count) {
			start_index = count > OLED_VISIBLE_ROWS	? count - OLED_VISIBLE_ROWS : 0;
	}

	item = page->p_child;

	for (uint8_t i = 0; i < start_index; i++) {
			item = item->p_next;
	}

	for (row = 0; row < OLED_VISIBLE_ROWS && item != NULL; row++) {
			uint8_t y = 16 + row * 8;

			u8g2_DrawStr(u8g2, 0, y, item == selected ? ">" : " ");
			u8g2_DrawStr(u8g2, 8, y, item->p_name);

			item = item->p_next;
	}
}

static void OLED_normal_page_render(u8g2_t *u8g2, const menuItem_t *page, const menuItem_t *item) {
	// 第一行：当前页面名称
	u8g2_DrawStr(u8g2, 0, 8, page->p_name);
	// 后面三行：当前页面的 child
	OLED_draw_items(u8g2, page, item);
}
	
void OLED_page_set_toggle_feedback(uint8_t LEDIndex) {
	s_feedbackValid = true;
	s_feedbackLED = LEDIndex;
}

void OLED_page_render(void) {
	u8g2_t *u8g2 = (u8g2_t *)OLED_service_get_u8g2_handle();
	if (u8g2 == NULL) {
		return;
	}
	const menuItem_t *page;
	const menuItem_t *item;

	page = menu_get_current_page();
	item = menu_get_current_item();

	if (page == NULL) {
			return;
	}

	u8g2_ClearBuffer(u8g2);

	switch(menu_get_mode()) {
		case MENU_MODE_LED_TOGGLE:
			OLED_LED_toggle_page_render(u8g2);
			break;
		case MENU_MODE_LOG:
			OLED_LOG_page_render(u8g2);
			break;
		case MENU_MODE_NORMAL:
			OLED_normal_page_render(u8g2, page, item);
			break;
		default:
			break;
	}
	
	u8g2_SendBuffer(u8g2);
}

void OLED_page_init(void){
	//将 OLED_page_render 注册给service hook
   OLED_service_register_render_cb(OLED_page_render);
}

