#include <OLED_page.h>
#include "stdio.h"
#include <menu_item.h>
#include <OLED_service.h>
#include <wavAnl_service.h>
#include "main.h"
#include "u8g2.h"

#define OLED_VISIBLE_ROWS 3

static uint8_t s_feedbackLED;
static bool s_feedbackValid = false;
	

static void OLED_value_page_render(u8g2_t *u8g2, const menuItem_t *item, bool editing) {
	char buf[12];
	int16_t value = editing ? menu_get_edit_value() : menu_item_value(item);
	snprintf(buf, sizeof(buf), "%d", value);

	u8g2_DrawStr(u8g2, 0, 8,  menu_item_name(item));
	u8g2_DrawStr(u8g2, 0, 16, buf);
	u8g2_DrawStr(u8g2, 0, 32, editing ? "Enter:OK  Back:Cancel" : "Enter:Edit  Back:Exit");
}

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

static void OLED_wave_param_page_render(u8g2_t *u8g2) {
	u8g2_DrawStr(u8g2, 0, 8, "Wave Param List");
	float period;
	float vpp;
	uint8_t index;
	const char* type = wavAnl_menu_info(&period, &vpp, &index);
	if (type == NULL) {
		u8g2_DrawStr(u8g2, 0, 24, "waiting for sampling...");
	}
	else {
		u8g2_DrawStr(u8g2, 0, 16, "wave type:");
		u8g2_DrawStr(u8g2, 64, 16, type);
		if (index != 255) {
			char buf[24];
			unsigned long period_x1000 = (unsigned long)(period * 1000.0f + 0.5f);
			snprintf(buf, sizeof(buf), "period: %lu.%03lu ms", period_x1000 / 1000U, period_x1000 % 1000U);
			u8g2_DrawStr(u8g2, 0, 24, buf);
			snprintf(buf, sizeof(buf), "vpp: %lu mV", (unsigned long)(vpp * 1000.0f + 0.5f));
			u8g2_DrawStr(u8g2, 0, 32, buf);
		}
	}
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

	for (item = menu_item_child(page); item != NULL; item = menu_item_next(item)) {
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

	item = menu_item_child(page);

	for (uint8_t i = 0; i < start_index; i++) {
			item = menu_item_next(item);
	}

	for (row = 0; row < OLED_VISIBLE_ROWS && item != NULL; row++) {
			uint8_t y = 16 + row * 8;

			u8g2_DrawStr(u8g2, 0, y, item == selected ? ">" : " ");
			u8g2_DrawStr(u8g2, 8, y, menu_item_name(item));

			item = menu_item_next(item);
	}
}

static void OLED_normal_page_render(u8g2_t *u8g2, const menuItem_t *page, const menuItem_t *item) {
	// 子菜单名
	u8g2_DrawStr(u8g2, 0, 8, menu_item_name(page));
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
		case MENU_MODE_VALUE_ADJUST:
			OLED_value_page_render(u8g2, item, menu_value_is_editing());
			break;
		case MENU_MODE_WAVE_PARAM:
			OLED_wave_param_page_render(u8g2);
			break;
		default:
			break;
	}
	
	u8g2_SendBuffer(u8g2);
}

void OLED_page_init(void){
   OLED_service_register_render_cb(OLED_page_render);
}

