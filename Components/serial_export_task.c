#include <serial_export_task.h>
#include <USART_task.h>
#include <ADC_service.h>
#include <memory_pool.h>
#include "stddef.h"

static serial_export_src_t s_src;
static void *s_ctx;
static serial_export_done_t s_done;
static uint32_t s_count;
static uint32_t s_idx;
static bool s_busy = false;
static int16_t *s_LUT;

bool serial_export_start(serial_export_src_t src, void *ctx, uint32_t count, serial_export_done_t on_done) {
	if (s_busy || count == 0) return false;
	s_src = src;
	s_ctx = ctx;
	s_count = count;
	s_done = on_done;
	s_idx = 0;
	s_busy = true;
	s_LUT = (int16_t *)MP_alloc(count * sizeof(int16_t));
  if (s_LUT == NULL) {
		USART_printf("[ERR] memory allocate failed, export stopped\r\n");
		s_busy = false;
    return false;
  }
	while (s_idx < s_count) {
		*(s_LUT + s_idx) = s_src(s_ctx, s_idx);
		s_idx++;
	}
	s_idx = 0;
	return true;
}

bool serial_export_busy(void) {
	return s_busy;
}

void serial_export_proc(void) {
	if (!s_busy) return;
	if (s_idx < s_count) {
		USART_printf("%d\r\n", (int)s_LUT[s_idx++]);
		return;
	}
	MP_free(s_LUT);
	s_busy = false;
	if (s_done != NULL) s_done();
}

static uint16_t ADC_wave_sample(void *ctx, uint32_t index) {
	const uint16_t *snap = ctx;
	return ADC_raw_to_mv(snap[index]);
}

void serial_export_ADC_wave(void) {
	uint32_t len = ADC_get_window_len();
	uint16_t *snap;

	if (serial_export_busy()) return;
	if (!ADC_is_window_ready()) {
		USART_printf("waveform: window not full, skip\r\n");
		return;
	}
	snap = MP_alloc(len * sizeof(uint16_t));
	if (snap == NULL) {
		USART_printf("[ERR] memory allocate failed, export stopped\r\n");
		return;
	}
	if (ADC_snapshot(snap, len)) {
		/* serial_export_start 会同步把数据拷进 s_LUT，所以快照可以马上释放 */
		serial_export_start(ADC_wave_sample, snap, len, NULL);
	}
	else {
		USART_printf("waveform: snapshot failed, export stopped\r\n");
	}
	MP_free(snap);
}
