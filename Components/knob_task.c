#include <knob_task.h>
#include "adc.h"
#include <menu_item.h>
#include <OLED_service.h>

#define KNOB_AVG_N       8
#define KNOB_TIMEOUT_MS  2U
#define KNOB_DEADBAND    24
#define KNOB_RAW_MIN     0
#define KNOB_RAW_MAX     4095

static uint32_t s_knobValue    = 0;   /* ����ƽ�����������ԭʼֵ */
static uint32_t s_knobValuePre = UINT32_MAX;  /* ��һ�α����ܵ�ֵ��ҵ��������� */
static knob_change_cb_t s_changeCb = NULL;

static HAL_StatusTypeDef knob_read_raw(uint32_t *out) {
	uint32_t sum = 0;
	for (uint8_t i = 0; i < KNOB_AVG_N; i++) {
		if (HAL_ADC_Start(&hadc2) != HAL_OK)
				return HAL_ERROR;
		if (HAL_ADC_PollForConversion(&hadc2, KNOB_TIMEOUT_MS) != HAL_OK)
				return HAL_TIMEOUT;
		sum += HAL_ADC_GetValue(&hadc2);
	}
	*out = (uint32_t)((sum + KNOB_AVG_N / 2) / KNOB_AVG_N);
	return HAL_OK;
}

void knob_proc(void) {
	uint32_t raw;
	if (knob_read_raw(&raw) != HAL_OK)
			return;

	if (raw - KNOB_RAW_MIN <= KNOB_DEADBAND) raw = KNOB_RAW_MIN;
	if (KNOB_RAW_MAX - raw <= KNOB_DEADBAND) raw = KNOB_RAW_MAX;
	s_knobValue = raw;

	int32_t diff = s_knobValue - s_knobValuePre;
	uint8_t atEnd = (s_knobValue == KNOB_RAW_MIN) || (s_knobValue == KNOB_RAW_MAX);
	uint8_t changed = (s_knobValuePre == UINT32_MAX) ||
										(diff > KNOB_DEADBAND) ||
										(diff < -KNOB_DEADBAND) ||
										(atEnd && diff != 0);

	if (changed) {
		s_knobValuePre = s_knobValue;
		if (s_changeCb != NULL) {
			s_changeCb(s_knobValue);
		}
	}

	if (menu_get_mode() == MENU_MODE_LOG) {
			if (changed) {
					OLED_printf(0, 2, 8, "%4ld", (long)s_knobValuePre);
			}
	}
}

uint32_t knob_get_value() {
	return s_knobValue;
}

void knob_register_change_cb(knob_change_cb_t fn_cb) {
	s_changeCb = fn_cb;
}