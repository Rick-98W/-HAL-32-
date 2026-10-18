#ifndef ADC_SERVICE_H
#define ADC_SERVICE_H

#include "stdint.h"
#include "stdbool.h"

#define ADC_WINDOW_SIZE 1000U

void ADC_init(void);
void ADC_set_TIM3_ARR(void);
void ADC_set_ratio(bool is);
bool ADC_get_ratio(void);
uint32_t ADC_raw_to_mv(uint16_t raw_val);
void ADC_freeze(void);
void ADC_resume(void);
const uint16_t* ADC_get_buffer(uint32_t* len, uint32_t* start);

#endif
