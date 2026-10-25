#include <ICAP_service.h>
#include "tim.h"
#include <USART_task.h>

#define ICAP_LEVEL_HIGH 1U
#define ICAP_LEVEL_LOW  0U

typedef struct {
	volatile uint16_t period;
	volatile uint16_t high;
	volatile uint8_t  valid;
	uint8_t  activeLevel;
	uint8_t  instanceNo;
	TIM_HandleTypeDef *htim;
} ICAPInstance_t;


// 实例注册
static ICAPInstance_t s_TIM9ICAP;
static ICAPInstance_t s_TIM12ICAP;

static void ICAP_start_instance(ICAPInstance_t *p_instance, uint8_t level){
	p_instance->period = 0;
	p_instance->high = 0;
	p_instance->valid = 0;
	p_instance->activeLevel = level;
	HAL_TIM_IC_Start_IT(p_instance->htim, TIM_CHANNEL_1);
	HAL_TIM_IC_Start_IT(p_instance->htim, TIM_CHANNEL_2);
}

void ICAP_init(void){
	s_TIM9ICAP.htim  = &htim9;
	s_TIM12ICAP.htim = &htim12;
	s_TIM9ICAP.instanceNo  = 1;
	s_TIM12ICAP.instanceNo = 2;
	ICAP_start_instance(&s_TIM9ICAP, ICAP_LEVEL_LOW);
	ICAP_start_instance(&s_TIM12ICAP, ICAP_LEVEL_LOW);
}


// 获取数据采集结果
static void ICAP_IRQ_disp(ICAPInstance_t *p_instance){
	/* PWM-input mode resets the counter on CH1 rising edges.  CH2 falling
		 * therefore completes the high-time belonging to the latest period. */
		if (p_instance->htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
			p_instance->period = (uint16_t)__HAL_TIM_GET_COMPARE(p_instance->htim, TIM_CHANNEL_1);
			p_instance->valid = 0;
		}
		else if (p_instance->htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
			p_instance->high = (uint16_t)__HAL_TIM_GET_COMPARE(p_instance->htim, TIM_CHANNEL_2);
			/* Ignore the startup edge and impossible pairs.  A valid pair is
			 * produced only after CH1 has reset the counter for this cycle. */
			p_instance->valid = (p_instance->high <= p_instance->period) ? 1U : 0U;
		}
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim){
	if (htim == s_TIM9ICAP.htim) 			 ICAP_IRQ_disp(&s_TIM9ICAP);
	else if (htim == s_TIM12ICAP.htim) ICAP_IRQ_disp(&s_TIM12ICAP);
}

// 串口传输数据采集结果
static void ICAP_parse_instance(ICAPInstance_t *p_instance) {
	uint16_t period, high;
	uint8_t  got_valid;
	__disable_irq();
	got_valid = p_instance->valid;
	period = p_instance->period;
	high = p_instance->high;
	p_instance->valid = 0;
	__enable_irq();
	
	if (!got_valid) return;
	if (period == 0) return;

	if (!p_instance->activeLevel) high = period - high;
	float duty = high * 100.0f / period;
	USART_printf("I%d:%f\n", p_instance->instanceNo, duty);
}

void ICAP_update(void){
	ICAP_parse_instance(&s_TIM9ICAP);
	ICAP_parse_instance(&s_TIM12ICAP);
}