#ifndef ADC_DAC_SYNC_H
#define ADC_DAC_SYNC_H

#include "stdint.h"
#include <DAC_service.h>

void ADC_DAC_sync_set_FSR(int16_t adjFSR);
void ADC_DAC_sync_set_period(int16_t adjPeriod);
void ADC_DAC_sync_set_wave(e_waveType_t waveType);

#endif
