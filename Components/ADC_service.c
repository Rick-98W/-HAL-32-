#include <ADC_service.h>
#include "adc.h"
#include "tim.h"
#include <DAC_service.h>
#include <USART_task.h>

static uint32_t s_Vrefint = 0;
static bool isDoubled = false;
static uint32_t s_writeIdx = 0;
static bool s_frozen = false;
static volatile bool s_wrapped = false;
uint16_t s_ADCWaveWindow[ADC_WINDOW_SIZE];

void ADC_set_TIM3_ARR(void) {
	uint32_t ARR = DAC_get_ARR();
	if (isDoubled) ARR /= 2;
  __HAL_TIM_SET_AUTORELOAD(&htim3, (uint32_t) ARR);
}

void ADC_init(void) {
	ADC->CCR |= ADC_CCR_TSVREFE;
	HAL_Delay(1);
	HAL_ADCEx_InjectedStart(&hadc1);
	if (HAL_ADCEx_InjectedPollForConversion(&hadc1, 10) == HAL_OK) {
			s_Vrefint = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
	}
	HAL_ADCEx_InjectedStop(&hadc1);
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)s_ADCWaveWindow, ADC_WINDOW_SIZE);
	ADC_set_TIM3_ARR();
	HAL_TIM_Base_Start(&htim3);
	USART_printf("vref=%lu cal=%u jdr=%lu\r\n",
             (unsigned long)s_Vrefint,
             (unsigned int)*VREFINT_CAL_ADDR,
             (unsigned long)hadc1.Instance->JDR1);
}

uint32_t ADC_get_calibrated_vdda(void) {
    if (s_Vrefint == 0) return 3300U;
    uint32_t cal = *VREFINT_CAL_ADDR;
    if (cal == 0xFFFFU) cal = 1500U; // 出厂校准值未写入时，用数据手册典型值
    uint32_t vdda = (3300UL * cal) / s_Vrefint;
    return vdda;
}

uint32_t ADC_raw_to_mv(uint16_t raw_val) {
    uint32_t vdda = ADC_get_calibrated_vdda();
    return ((uint32_t)raw_val * vdda) / 4095U;
}

bool ADC_freeze(void) {
	if (s_frozen) return false;
	s_frozen = true;
	HAL_TIM_Base_Stop(&htim3);
	HAL_ADC_Stop_DMA(&hadc1);
	s_writeIdx = ADC_WINDOW_SIZE - __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);
	return true;
}

void ADC_resume(void) {
	if (!s_frozen) return;
	s_frozen = false;
	s_wrapped = false;
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)s_ADCWaveWindow, ADC_WINDOW_SIZE);
	HAL_TIM_Base_Start(&htim3);
}

/* Pure read: does not freeze or restart DMA. */
bool ADC_is_window_ready(void) {
	if (s_wrapped) return true;
	uint32_t len = isDoubled ? ADC_WINDOW_SIZE : ADC_WINDOW_SIZE / 2U;
	return (ADC_WINDOW_SIZE - __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle)) >= len;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
	if (hadc == &hadc1) s_wrapped = true;
}

void ADC_reset_window(void) {
	if (s_frozen) return;
	HAL_ADC_Stop_DMA(&hadc1);
	s_wrapped = false;
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)s_ADCWaveWindow, ADC_WINDOW_SIZE);
}

void ADC_set_ratio(bool is) {
	if (isDoubled == is) return;
	isDoubled = is;
	ADC_set_TIM3_ARR();
	ADC_reset_window();
}

bool ADC_get_ratio(void) {
	return isDoubled;
}

const uint16_t* ADC_get_buffer(uint32_t* len, uint32_t* start) {
	*len = isDoubled ? ADC_WINDOW_SIZE : ADC_WINDOW_SIZE / 2U;
	*start = (s_writeIdx + ADC_WINDOW_SIZE - *len) % ADC_WINDOW_SIZE;
	return s_ADCWaveWindow;
}