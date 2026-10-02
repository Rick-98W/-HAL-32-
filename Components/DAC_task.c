#include "DAC_task.h"
#include "dac.h"
#include "tim.h"
#include <math.h>
#define DAC_LUT_SIZE 256U
#define DAC_MID 2048U
#define DAC_MAX 4095U
static uint16_t s_lut[DAC_LUT_SIZE];
static volatile uint16_t s_index;
static volatile DAC_Waveform_t s_waveform = DAC_WAVE_SINE;
static volatile uint16_t s_period_points = 64U;
static volatile uint8_t s_vpp_percent = 80U;
static uint8_t s_initialized;
static uint16_t clamp_u16(int32_t value) { if (value < 0) return 0U; if (value > DAC_MAX) return DAC_MAX; return (uint16_t)value; }
static void DAC_rebuild_lut(void)
{
  uint32_t i; float amplitude = (float)DAC_MID * (float)s_vpp_percent / 100.0f;
  for (i = 0U; i < DAC_LUT_SIZE; ++i) {
    float phase = (float)i / (float)DAC_LUT_SIZE; float normalized;
    if (s_waveform == DAC_WAVE_SINE) normalized = sinf(phase * 2.0f * 3.14159265f);
    else if (s_waveform == DAC_WAVE_SQUARE) normalized = (phase < 0.5f) ? 1.0f : -1.0f;
    else normalized = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
    s_lut[i] = clamp_u16((int32_t)((float)DAC_MID + normalized * amplitude * 0.5f));
  }
}
void DAC_task_init(void) { DAC_rebuild_lut(); s_index = 0U; s_initialized = 1U; HAL_DAC_Start(&hdac, DAC_CHANNEL_1); HAL_TIM_Base_Start_IT(&htim6); }
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6 && s_initialized) {
    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, s_lut[s_index]);
    s_index = (uint16_t)((s_index + (DAC_LUT_SIZE / s_period_points)) % DAC_LUT_SIZE);
  }
}
void DAC_proc(void) { }
void DAC_on_key(uint8_t key_id)
{
  if (key_id == 0U && s_period_points < 256U) s_period_points <<= 1U;
  if (key_id == 1U && s_period_points > 16U) s_period_points >>= 1U;
  if (key_id == 2U) { s_waveform = (DAC_Waveform_t)((s_waveform + 1U) % 3U); DAC_rebuild_lut(); }
}
void DAC_set_waveform(DAC_Waveform_t waveform) { if (waveform <= DAC_WAVE_TRIANGLE) { s_waveform = waveform; DAC_rebuild_lut(); } }
void DAC_set_period_points(uint16_t points) { if (points == 16U || points == 32U || points == 64U || points == 128U || points == 256U) s_period_points = points; }
void DAC_set_vpp_percent(uint8_t percent) { if (percent > 100U) percent = 100U; s_vpp_percent = percent; DAC_rebuild_lut(); }
DAC_Waveform_t DAC_get_waveform(void) { return s_waveform; }
uint16_t DAC_get_period_points(void) { return s_period_points; }
uint8_t DAC_get_vpp_percent(void) { return s_vpp_percent; }
const char *DAC_waveform_name(void) { if (s_waveform == DAC_WAVE_SQUARE) return "SQUARE"; if (s_waveform == DAC_WAVE_TRIANGLE) return "TRIANGLE"; return "SINE"; }
