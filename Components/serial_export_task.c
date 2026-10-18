#include <serial_export_task.h>
#include <USART_task.h>
#include <ADC_service.h>

typedef struct {
	const uint16_t* buf;
	uint32_t start;
} ADC_wave_ctx_t;

static serial_export_src_t s_src;
static void *s_ctx;
static serial_export_done_t s_done;
static uint32_t s_count;
static uint32_t s_idx;
static bool s_busy = false;
static ADC_wave_ctx_t s_ADC_wave;

bool serial_export_start(serial_export_src_t src, void *ctx, uint32_t count, serial_export_done_t on_done) {
	if (s_busy) return false;
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
		USART_printf("%lu\n", (unsigned long)s_src(s_ctx, s_idx++));
		return;
	}
	s_busy = false;
	s_done();
}

static uint32_t ADC_wave_sample(void *ctx, uint32_t index) {
	const ADC_wave_ctx_t *wave = ctx;
	return ADC_raw_to_mv(wave->buf[(wave->start + index) % ADC_WINDOW_SIZE]);
}

void serial_export_ADC_wave(void) {
	uint32_t len;
	uint32_t start;
	if (serial_export_busy()) return;
	ADC_freeze();
	s_ADC_wave.buf = ADC_get_buffer(&len, &start);
	s_ADC_wave.start = start;
	serial_export_start(ADC_wave_sample, &s_ADC_wave, len, ADC_resume);
}