#include <serial_export_task.h>
#include <USART_task.h>
#include "stddef.h"

static serial_export_src_t s_src;
static void *s_ctx;
static serial_export_done_t s_done;
static uint32_t s_count;
static uint32_t s_idx;
static bool s_busy = false;

bool serial_export_start(serial_export_src_t src, void *ctx, uint32_t count, serial_export_done_t on_done) {
	if (s_busy || src == NULL || count == 0) return false;
	s_src = src;
	s_ctx = ctx;
	s_count = count;
	s_done = on_done;
	s_idx = 0;
	s_busy = true;
	return true;
}

bool serial_export_busy(void) {
	return s_busy;
}

void serial_export_proc(void) {
	if (!s_busy) return;
	if (s_idx < s_count) {
		USART_printf("%l\r\n", (long)s_src(s_ctx, s_idx++));
		return;
	}
	s_busy = false;
	if (s_done != NULL) s_done();
}
