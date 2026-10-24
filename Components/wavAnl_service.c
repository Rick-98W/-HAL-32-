#include <wavAnl_service.h>
#include <waveform_analyzer.h>
#include <ADC_service.h>
#include <USART_task.h>
#include <memory_pool.h>
#include "tim.h"

#if ADC_WINDOW_SIZE > WAVE_MAX_LEN
#error "ADC_WINDOW_SIZE exceeds WAVE_MAX_LEN"
#endif

static waveformInfo s_info = {.waveform_type = ADC_WAVEFORM_UNKNOWN};
static bool s_has_result = false;
static uint32_t s_resultGen = 0;    /* 分析时的 ADC 窗口代数，代数变了说明结果已过期 */

static float ADC_window_sample(void *ctx, uint32_t index) {
    const uint16_t *snap = ctx;
    return (float)ADC_raw_to_mv(snap[index]) * 0.001f;
}

static float wave_sample_interval_s(void) {
    uint32_t tim_clk = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        tim_clk *= 2U;
    }
    tim_clk /= (htim3.Instance->PSC + 1U);
    return (float)(__HAL_TIM_GET_AUTORELOAD(&htim3) + 1U) / (float)tim_clk;
}

bool wavAnl_update(void) {
    uint32_t len = ADC_get_window_len();
    uint16_t *snap;

    /* 失败时保留上一次的结果，是否还有效由 wavAnl_is_valid() 判断 */
    if (!ADC_is_window_ready()) {
        USART_printf("waveform: window not full, skip\r\n");
        return false;
    }
    snap = MP_alloc(len * sizeof(uint16_t));
    if (snap == NULL) {
        USART_printf("[ERR] waveform: memory allocate failed, skip\r\n");
        return false;
    }
    if (!ADC_snapshot(snap, len)) {
        MP_free(snap);
        USART_printf("waveform: snapshot failed, skip\r\n");
        return false;
    }
    s_info = wave_analyze(ADC_window_sample, snap, len, wave_sample_interval_s());
    MP_free(snap);
    s_has_result = true;
    s_resultGen = ADC_get_generation();
    return true;
}

const waveformInfo *wavAnl_get_info(void) {
    return &s_info;
}

static const char* wave_type_name(ADC_WaveformType e_waveform) {
  switch (e_waveform) {
    case ADC_WAVEFORM_DC:       return "DC";
    case ADC_WAVEFORM_SINE:     return "SINE";
    case ADC_WAVEFORM_SQUARE:   return "SQR";
    case ADC_WAVEFORM_TRIANGLE: return "TRI";
    case ADC_WAVEFORM_SAWTOOTH: return "SAW";
    default:                    return "UNKNOWN";
    }
}

void wavAnl_print(void) {
	if(!wavAnl_update()) return;
    const waveformInfo *temp_info = &s_info;
    unsigned long freq_x10 = (unsigned long)(temp_info->frequency * 10.0f + 0.5f);
    unsigned long period_x1000 = (unsigned long)(temp_info->period * 1000.0f + 0.5f);
    unsigned long vpp_mv = (unsigned long)(temp_info->vpp * 1000.0f + 0.5f);
    unsigned long mean_mv = (unsigned long)(temp_info->mean * 1000.0f + 0.5f);
    unsigned long rms_mv = (unsigned long)(temp_info->rms * 1000.0f + 0.5f);
    unsigned long h2_pct = (unsigned long)(temp_info->h2_ratio * 100.0f + 0.5f);
    unsigned long h3_pct = (unsigned long)(temp_info->h3_ratio * 100.0f + 0.5f);
    unsigned long h5_pct = (unsigned long)(temp_info->h5_ratio * 100.0f + 0.5f);

    USART_printf("waveform: type=%s freq=%lu.%lu Hz period=%lu.%03lu ms vpp=%lu mV mean=%lu mV rms=%lu mV h2=%lu%% h3=%lu%% h5=%lu%%\r\n",
                 wave_type_name(temp_info->waveform_type),
                 freq_x10 / 10U, freq_x10 % 10U,
                 period_x1000 / 1000U, period_x1000 % 1000U,
                 vpp_mv, mean_mv, rms_mv,
                 h2_pct, h3_pct, h5_pct);
}

bool wavAnl_is_valid(void) {
    return s_has_result && s_resultGen == ADC_get_generation();
}

float wavAnl_get_freq(void) {
    return wavAnl_is_valid() ? s_info.frequency : 0.0f;
}

float wavAnl_get_period(void) {
    return wavAnl_is_valid() ? s_info.period : 0.0f;
}

float wavAnl_get_vpp(void) {
    return wavAnl_is_valid() ? s_info.vpp : 0.0f;
}

const char* wavAnl_get_type_name(void) {
    if (!wavAnl_is_valid()) return "UNKNOWN";
    return wave_type_name(s_info.waveform_type);
}

const char* wavAnl_menu_info(float* period, float* vpp, uint8_t* index) {
	if (!wavAnl_is_valid()) return NULL;
	*index = s_info.waveform_type;
	*period = s_info.period;
	*vpp = s_info.vpp;
	return wave_type_name(s_info.waveform_type);
}
