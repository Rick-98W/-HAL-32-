#include <ICAP_task.h>
#include "tim.h"
#include <USART_task.h>

static volatile uint16_t s_Period;
static volatile uint16_t s_High;
static volatile uint8_t s_HighReady;
static volatile uint8_t s_CaptureStarted;

static void ICAP_start(void){
	if (s_CaptureStarted) return;
	s_Period = 0;
	s_High = 0;
	s_HighReady = 0;
	if (HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1) != HAL_OK)
		return;
	if (HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2) != HAL_OK) {
		HAL_TIM_IC_Stop_IT(&htim3, TIM_CHANNEL_1);
		return;
	}
	s_CaptureStarted = 1;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim){
	if (htim != &htim3) return;
	/* PWM-input mode resets the counter on CH1 rising edges.  CH2 falling
	 * therefore completes the high-time belonging to the latest period. */
	if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
		s_Period = (uint16_t)__HAL_TIM_GET_COMPARE(htim, TIM_CHANNEL_1);
		s_HighReady = 0;
	}
	else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
		s_High = (uint16_t)__HAL_TIM_GET_COMPARE(htim, TIM_CHANNEL_2);
		/* Ignore the startup edge and impossible pairs.  A valid pair is
		 * produced only after CH1 has reset the counter for this cycle. */
		s_HighReady = (s_High <= s_Period) ? 1U : 0U;
	}
}

void ICAP_proc(void){
	if (!s_CaptureStarted) ICAP_start();
	if (s_HighReady) {
		uint16_t period;
		uint16_t high;
		__disable_irq();
		period = s_Period;
		high = s_High;
		s_HighReady = 0;
		__enable_irq();
//		if (period != 0U && high <= period) {
			float duty = high * 100.0f / period;
			USART_printf("%f\n", duty);
//		}
	}
}
