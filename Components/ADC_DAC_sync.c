#include <ADC_DAC_sync.h>
#include <DAC_service.h>
#include <ADC_service.h>

/* Any change to the DAC output invalidates the ADC window, so the next
   analysis has to wait for fresh samples. Setting the same value is a no-op. */

void ADC_DAC_sync_set_FSR(int16_t adjFSR) {
    int16_t prev = DAC_get_FSR();
    DAC_set_FSR(adjFSR);
    if (DAC_get_FSR() != prev) {
        ADC_reset_window();
    }
}

void ADC_DAC_sync_set_period(int16_t adjPeriod) {
    DAC_set_period(adjPeriod);
    ADC_set_TIM3_ARR();
    ADC_reset_window();
}

void ADC_DAC_sync_set_wave(e_waveType_t waveType) {
    e_waveType_t prev = DAC_get_waveType();
    DAC_set_wave(waveType);
    if (DAC_get_waveType() != prev) {
        ADC_reset_window();
    }
}
