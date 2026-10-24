#ifndef ADC_SERVICE_H
#define ADC_SERVICE_H

#include "stdint.h"
#include "stdbool.h"

#define ADC_WINDOW_SIZE 1024U

void ADC_init(void);
void ADC_set_TIM3_ARR(void);
void ADC_reset_window(void);
void ADC_set_ratio(bool is);
bool ADC_get_ratio(void);
uint32_t ADC_get_window_len(void);
uint32_t ADC_raw_to_mv(uint16_t raw_val);
bool ADC_is_window_ready(void);
bool ADC_snapshot(uint16_t *dst, uint32_t len);

#endif
