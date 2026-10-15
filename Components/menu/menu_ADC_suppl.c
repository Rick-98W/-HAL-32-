#include <stddef.h>

#include <menu_tmpl.h>
#include <menu_ADC_suppl.h>
#include <DAC_service.h>

static menuItem_t s_DACSetFSR;
static menuItem_t s_DACSetPeriod;
static menuItem_t s_DACSetWave;
static menuItem_t s_DACSetSINUSOIDAL;
static menuItem_t s_DACSetTRIANGULAR;
static menuItem_t s_DACSetSAWTOOTH;


int16_t s_DACFSR = 3300;
int16_t s_DACPeriod = 5000;

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
static void DAC_set_sawtooth(void) {DAC_set_wave(SAWTOOTH);}

void menu_ADC_suppl_init(menuItem_t *parent) {
	//											NODE								NAME						PAGEID			NEXT									PREV								PARENT					CHILD								
	menu_create_submenu(&s_DACSetFSR,				 "DACSetFSR",			DACSetFSR,    &s_DACSetPeriod,		 NULL,								parent,				 NULL);
	menu_bind_value(&s_DACSetFSR, &s_DACFSR, 0, 3300, menu_value_enter, DAC_set_FSR);
	menu_create_submenu(&s_DACSetPeriod,		 "DACSetPeriod",	DACSetPeriod,	&s_DACSetWave,			 &s_DACSetFSR,				parent,				 NULL);
	menu_bind_value(&s_DACSetPeriod, &s_DACPeriod, 5, 5400, menu_value_enter, DAC_set_period);
	menu_create_submenu(&s_DACSetWave,			 "DACSetWave",		DACSetWave,		NULL,								 &s_DACSetPeriod,			parent,				 &s_DACSetSINUSOIDAL);
	menu_bind_enter(&s_DACSetWave, DAC_set_wave_enter);
	menu_create_submenu(&s_DACSetSINUSOIDAL, "sinusodial",		NULL_PAGE,		&s_DACSetTRIANGULAR, NULL,								&s_DACSetWave, NULL);
	menu_bind_action(&s_DACSetSINUSOIDAL, DAC_set_sinusodial);
	menu_create_submenu(&s_DACSetTRIANGULAR, "triangular",		NULL_PAGE, 		&s_DACSetSAWTOOTH,	 &s_DACSetSINUSOIDAL, &s_DACSetWave, NULL);
	menu_bind_action(&s_DACSetTRIANGULAR, DAC_set_triangular);
	menu_create_submenu(&s_DACSetSAWTOOTH,   "sawtooth",			NULL_PAGE, 		NULL,								 &s_DACSetTRIANGULAR, &s_DACSetWave, NULL);
	menu_bind_action(&s_DACSetSAWTOOTH, DAC_set_sawtooth);
	
	
	
	parent->p_child = &s_DACSetFSR;
}