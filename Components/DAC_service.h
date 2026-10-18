#ifndef DAC_SERVICE_H
#define DAC_SERVICE_H

#include "stdint.h"

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
	
#endif