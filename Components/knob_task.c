#include <knob_task.h>
#include "adc.h"
#include <menu_item.h>
#include <OLED_service.h>

#define KNOB_AVG_N       8
#define KNOB_TIMEOUT_MS  2U
#define KNOB_DEADBAND    14
#define KNOB_RAW_MIN     0
#define KNOB_RAW_MAX     4095

static int32_t s_knobValue    = 0;   /* 本轮平均并吸附后的原始值 */
static int32_t s_knobValuePre = -1;  /* 上一次被接受的值（业务用这个） */

static HAL_StatusTypeDef knob_read_raw(int32_t *out) {
	uint32_t sum = 0;
	for (uint8_t i = 0; i < KNOB_AVG_N; i++) {
		if (HAL_ADC_Start(&hadc2) != HAL_OK)
				return HAL_ERROR;
		if (HAL_ADC_PollForConversion(&hadc2, KNOB_TIMEOUT_MS) != HAL_OK)
				return HAL_TIMEOUT;
		sum += HAL_ADC_GetValue(&hadc2);
	}
	*out = (int32_t)((sum + KNOB_AVG_N / 2) / KNOB_AVG_N);
	return HAL_OK;
}

void knob_proc(void) {
	int32_t raw;
	if (knob_read_raw(&raw) != HAL_OK)
			return;

	if (raw - KNOB_RAW_MIN <= KNOB_DEADBAND) raw = KNOB_RAW_MIN;
	if (KNOB_RAW_MAX - raw <= KNOB_DEADBAND) raw = KNOB_RAW_MAX;
	s_knobValue = raw;

	int32_t diff = s_knobValue - s_knobValuePre;
	uint8_t atEnd = (s_knobValue == KNOB_RAW_MIN) || (s_knobValue == KNOB_RAW_MAX);
	uint8_t changed = (s_knobValuePre < 0) ||
										(diff > KNOB_DEADBAND) ||
										(diff < -KNOB_DEADBAND) ||
										(atEnd && diff != 0);

	if (changed)
			s_knobValuePre = s_knobValue;

	if (menu_get_mode() == MENU_MODE_LOG) {
			if (changed) {
					OLED_printf(0, 2, 8, "%4ld", (long)s_knobValuePre);
			}
	}
}