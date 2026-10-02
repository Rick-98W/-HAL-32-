#ifndef DAC_TASK_H
#define DAC_TASK_H
#include <stdint.h>
typedef enum { DAC_WAVE_SINE = 0, DAC_WAVE_SQUARE, DAC_WAVE_TRIANGLE } DAC_Waveform_t;
void DAC_task_init(void);
void DAC_proc(void);
void DAC_on_key(uint8_t key_id);
void DAC_set_waveform(DAC_Waveform_t waveform);
void DAC_set_period_points(uint16_t points);
void DAC_set_vpp_percent(uint8_t percent);
DAC_Waveform_t DAC_get_waveform(void);
uint16_t DAC_get_period_points(void);
uint8_t DAC_get_vpp_percent(void);
const char *DAC_waveform_name(void);
#endif
