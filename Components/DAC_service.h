#ifndef DAC_SERVICE_H
#define DAC_SERVICE_H

#include "stdint.h"

#define DAC_FSR_MIN     0
#define DAC_FSR_MAX     3300
#define DAC_PERIOD_MIN  5
#define DAC_PERIOD_MAX  5400

typedef enum {
  SINUSOIDAL,
  TRIANGULAR,
  SAWTOOTH
} e_waveType_t;

void DAC_init(void);
void DAC_set_FSR(int16_t adjFSR);
void DAC_set_period(int16_t adjPeriod);
void DAC_set_wave(e_waveType_t waveType);
e_waveType_t DAC_get_waveType(void);
uint32_t DAC_get_ARR(void);
int16_t DAC_get_FSR(void);
int16_t DAC_get_period(void);

/* 输出实际变化（FSR / period / wave）后回调，参数是当前的定时器 ARR。注册时会立即回放一次。 */
typedef void (*DAC_change_cb_t)(uint32_t dacARR);
void DAC_register_change_cb(DAC_change_cb_t fn_cb);
	
#endif