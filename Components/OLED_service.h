#ifndef OLED_SERVICE_H
#define OLED_SERVICE_H

#include "stdint.h"

typedef void (*OLED_render_cb_t)(void);

void OLED_init(void);
void OLED_proc(void);
void OLED_printf(uint8_t x, uint8_t y, uint8_t fontsize, const char *fmt, ...);
void OLED_task_poll(void);
void OLED_request_refresh(uint32_t delay_ms);
	
void OLED_service_register_render_cb(OLED_render_cb_t fn_render);
void *OLED_service_get_u8g2_handle(void);
	
#endif