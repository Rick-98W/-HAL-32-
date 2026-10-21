#include <menu_ADC_suppl.h>
#include <menu_tmpl.h>
#include <DAC_service.h>
#include <ADC_service.h>
#include <serial_export_task.h>
#include <wavAnl_service.h>
#include "stddef.h"


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


int16_t s_DACFSR = 3300;
int16_t s_DACPeriod = 5000;

static void DAC_set_period_and_ADC_sync(int16_t adjPeriod) {
	DAC_set_period(adjPeriod);
	ADC_set_TIM3_ARR();
	ADC_reset_window();
}

static void DAC_set_wave_enter(void) {
	switch(DAC_get_waveType()) {
		case SINUSOIDAL: menu_focus_item(&s_DACSetSINUSOIDAL); break;
		case TRIANGULAR: menu_focus_item(&s_DACSetTRIANGULAR); break;
		case SAWTOOTH:   menu_focus_item(&s_DACSetSAWTOOTH);   break;
		default:         break;
	}
}

static void DAC_set_sinusodial(void) {DAC_set_wave(SINUSOIDAL);}
static void DAC_set_triangular(void) {DAC_set_wave(TRIANGULAR);}
static void DAC_set_sawtooth(void)	 {DAC_set_wave(SAWTOOTH);}

static void ADC_set_sampl_ratio_enter(void) {
	if (ADC_get_ratio()) menu_focus_item(&s_ADCSetDoubleSampl);
	else								 menu_focus_item(&s_ADCSetSingleSampl);
}

static void ADC_set_single_sampling(void) {ADC_set_ratio(0);}
static void ADC_set_double_sampling(void) {ADC_set_ratio(1);}

static void ADC_get_wave_param(void) {
	wavAnl_update();
	menu_ADC_param_enter();
}

void menu_ADC_suppl_init(menuItem_t *parent) {
	//											NODE								NAME									PAGEID						NEXT									PREV									PARENT								CHILD								
	menu_create_submenu(&s_DACSetFSR,					"DACSetFSR",					DACSetFSR,					&s_DACSetPeriod,			NULL,									parent,								NULL);
	menu_bind_value(&s_DACSetFSR, &s_DACFSR, 0, 3300, menu_value_enter, DAC_set_FSR);
	
	menu_create_submenu(&s_DACSetPeriod,			"DACSetPeriod",				DACSetPeriod,				&s_DACSetWave,				&s_DACSetFSR,					parent,								NULL);
	menu_bind_value(&s_DACSetPeriod, &s_DACPeriod, 5, 5400, menu_value_enter, DAC_set_period_and_ADC_sync);
	
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
	menu_bind_action(&s_ADCExport, serial_export_ADC_wave);	
	
	menu_create_submenu(&s_ADCWaveParam,			"ADCWaveParam",				ADCWaveParam,				NULL,									&s_ADCExport,					parent,								NULL);
	menu_bind_action(&s_ADCWaveParam, ADC_get_wave_param);
	
	
	parent->p_child = &s_DACSetFSR;
}