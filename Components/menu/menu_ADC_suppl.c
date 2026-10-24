#include <menu_ADC_suppl.h>
#include <menu_tmpl.h>
#include <DAC_service.h>
#include <ADC_service.h>
#include <memory_pool.h>
#include <serial_export_task.h>
#include <wavAnl_service.h>
#include <OLED_service.h>
#include "stddef.h"

#include <USART_task.h>

static menuItem_t s_DACSetFSR;
static menuItem_t s_DACSetPeriod;
static menuItem_t s_DACSetWave;
static menuItem_t s_DACSetSINUSOIDAL;
static menuItem_t s_DACSetTRIANGULAR;
static menuItem_t s_DACSetSAWTOOTH;
static menuItem_t s_ADCSetSamplFactor;
static menuItem_t s_ADCSetSingleSampl;
static menuItem_t s_ADCSetDoubleSampl;
static menuItem_t s_ADCExport;
static menuItem_t s_ADCWaveParam;

static int16_t s_DACFSRView = 3300;
static int16_t s_DACPeriodView = 5000;

static void DAC_waveType_disp_calibrate(void) {
	switch(DAC_get_waveType()) {
		case SINUSOIDAL: menu_focus_item(&s_DACSetSINUSOIDAL); break;
		case TRIANGULAR: menu_focus_item(&s_DACSetTRIANGULAR); break;
		case SAWTOOTH:   menu_focus_item(&s_DACSetSAWTOOTH);   break;
		default:         break;
	}
}

static void DAC_set_wave_enter(void) {DAC_waveType_disp_calibrate();}

static void DAC_set_sinusodial(void) {DAC_set_wave(SINUSOIDAL);}
static void DAC_set_triangular(void) {DAC_set_wave(TRIANGULAR);}
static void DAC_set_sawtooth(void)	 {DAC_set_wave(SAWTOOTH);}


static void ADC_ratio_disp_calibrate(void) {
	if (ADC_get_ratio()) menu_focus_item(&s_ADCSetDoubleSampl);
	else								 menu_focus_item(&s_ADCSetSingleSampl);
}

static void ADC_set_sampl_ratio_enter(void) {ADC_ratio_disp_calibrate();}

static void ADC_set_single_sampling(void) {ADC_set_ratio(0);}
static void ADC_set_double_sampling(void) {ADC_set_ratio(1);}

static void ADC_get_wave_param(void) {
	wavAnl_update();
	menu_ADC_param_enter();
}

/* 应用层适配：把 ADC 窗口拷贝成快照，交给通用的 serial_export 逐行输出。
   快照由本文件持有，导出结束或启动失败时释放。 */
static uint16_t *s_exportSnap = NULL;

static uint16_t ADC_export_sample(void *ctx, uint32_t index) {
	const uint16_t *snap = ctx;
	return (uint16_t)ADC_raw_to_mv(snap[index]);
}

static void ADC_export_release(void) {
	MP_free(s_exportSnap);
	s_exportSnap = NULL;
}

void ADC_wave_export_start(void) {
	uint32_t len = ADC_get_window_len();

	if (serial_export_busy()) return;
	if (!ADC_is_window_ready()) {
		USART_printf("waveform: window not full, skip\r\n");
		return;
	}
	s_exportSnap = MP_alloc(len * sizeof(uint16_t));
	if (s_exportSnap == NULL) {
		USART_printf("[ERR] memory allocate failed, export stopped\r\n");
		return;
	}
	if (!ADC_snapshot(s_exportSnap, len)) {
		USART_printf("waveform: snapshot failed, export stopped\r\n");
		ADC_export_release();
		return;
	}
	if (!serial_export_start(ADC_export_sample, s_exportSnap, len, ADC_export_release)) {
		ADC_export_release();
	}
}

void menu_ADC_suppl_init(menuItem_t *parent) {
	//											NODE								NAME									PAGEID						NEXT									PREV									PARENT								CHILD								
	menu_create_submenu(&s_DACSetFSR,					"DACSetFSR",					DACSetFSR,					&s_DACSetPeriod,			NULL,									parent,								NULL);
	menu_bind_value(&s_DACSetFSR, &s_DACFSRView, DAC_FSR_MIN, DAC_FSR_MAX, menu_value_enter, DAC_set_FSR);
	
	menu_create_submenu(&s_DACSetPeriod,			"DACSetPeriod",				DACSetPeriod,				&s_DACSetWave,				&s_DACSetFSR,					parent,								NULL);
	menu_bind_value(&s_DACSetPeriod, &s_DACPeriodView, DAC_PERIOD_MIN, DAC_PERIOD_MAX, menu_value_enter, DAC_set_period);
	
	menu_create_submenu(&s_DACSetWave,				"DACSetWave",					DACSetWave,					&s_ADCSetSamplFactor,	&s_DACSetPeriod,			parent,								&s_DACSetSINUSOIDAL);
	menu_bind_enter(&s_DACSetWave, DAC_set_wave_enter);
	menu_create_submenu(&s_DACSetSINUSOIDAL,	"sinusodial",					NULL_PAGE,					&s_DACSetTRIANGULAR,	NULL,									&s_DACSetWave,				NULL);
	menu_bind_action(&s_DACSetSINUSOIDAL, DAC_set_sinusodial);
	menu_create_submenu(&s_DACSetTRIANGULAR,	"triangular",					NULL_PAGE,					&s_DACSetSAWTOOTH,		&s_DACSetSINUSOIDAL,	&s_DACSetWave,				NULL);
	menu_bind_action(&s_DACSetTRIANGULAR, DAC_set_triangular);
	menu_create_submenu(&s_DACSetSAWTOOTH,		"sawtooth",						NULL_PAGE,					NULL,									&s_DACSetTRIANGULAR,	&s_DACSetWave,				NULL);
	menu_bind_action(&s_DACSetSAWTOOTH, DAC_set_sawtooth);
	
	menu_create_submenu(&s_ADCSetSamplFactor, "ADCSetSamplFactor",	ADCSetSamplFactor,	&s_ADCExport,					&s_DACSetWave,				parent,								&s_ADCSetSingleSampl);
	menu_bind_enter(&s_ADCSetSamplFactor, ADC_set_sampl_ratio_enter);
	menu_create_submenu(&s_ADCSetSingleSampl,	"single",							NULL_PAGE,					&s_ADCSetDoubleSampl,	NULL,									&s_ADCSetSamplFactor, NULL);
	menu_bind_action(&s_ADCSetSingleSampl, ADC_set_single_sampling);
	menu_create_submenu(&s_ADCSetDoubleSampl,	"double",							NULL_PAGE,					NULL,								 	&s_ADCSetSingleSampl,	&s_ADCSetSamplFactor, NULL);
	menu_bind_action(&s_ADCSetDoubleSampl, ADC_set_double_sampling);
	
	menu_create_submenu(&s_ADCExport,					"ADCExport",					ADCExport,					&s_ADCWaveParam,			&s_ADCSetSamplFactor,	parent,								NULL);
	menu_bind_action(&s_ADCExport, ADC_wave_export_start);
	
	menu_create_submenu(&s_ADCWaveParam,			"ADCWaveParam",				ADCWaveParam,				NULL,									&s_ADCExport,					parent,								NULL);
	menu_bind_action(&s_ADCWaveParam, ADC_get_wave_param);
	
	parent->p_child = &s_DACSetFSR;
}

//=================USART cmd sync callback=============================================

void DAC_menu_refresh_FSR(void) {
	s_DACFSRView = DAC_get_FSR();
	if ((menu_get_editing_item() == &s_DACSetFSR) && !menu_value_is_editing()) {
		OLED_proc();
	}
}

void DAC_menu_refresh_period(void) {
	s_DACPeriodView = DAC_get_period();
	if ((menu_get_editing_item() == &s_DACSetPeriod) && !menu_value_is_editing()) {
		OLED_proc();
	}
}

void DAC_menu_refresh_waveType(void) {
	if (menu_get_current_page()->page_ID == DACSetWave) {
		DAC_waveType_disp_calibrate();
		OLED_proc();
	}
}

void ADC_menu_refresh_wave(void) {
	if (menu_get_mode() == MENU_MODE_WAVE_PARAM) {
		OLED_proc();
	}
}

void ADC_menu_refresh_ratio(void) {
	if (menu_get_current_page()->page_ID == ADCSetSamplFactor) {
		ADC_ratio_disp_calibrate();
		OLED_proc();
	}
}