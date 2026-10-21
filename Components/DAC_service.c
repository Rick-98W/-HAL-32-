#include <DAC_service.h>
#include "dac.h"
#include "tim.h"
#include "math.h"

#define PI_2 (6.2831853f)
#define PI_1 (3.1415926f)
#define DAC_LUT_LENGTH 256U

static uint16_t s_DACLUT[3][DAC_LUT_LENGTH];
static uint32_t s_currFSR = 3300;
static uint32_t s_currPeriod = 5000;
static uint64_t targetARR;

static e_waveType_t s_waveMode = SINUSOIDAL;

/**
 * @brief  ��̬���� DAC ������ε�����
 * @param  u_periodMs Ŀ�겨�����ڣ���λ���� (ms)
 */
void DAC_set_waveform_period_ms(uint32_t periodMs) {
    if (periodMs == 0) return;

    // 1. ��ȡ APB1 ����ʱ��
    uint32_t PCLK1 = HAL_RCC_GetPCLK1Freq();
    uint32_t TIM6BaseCLK;

    // 2. ���� APB1 Ԥ��Ƶϵ������ TIM6 ������ʱ�� (��Ƶϵ�� != 1 ʱ����ʱ��ʱ���Զ� x2���� 90MHz)
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        TIM6BaseCLK = PCLK1 * 2U;
    }
    else {
        TIM6BaseCLK = PCLK1;
    }

    // 3. �� htim6 ���ֱ�Ӷ�ȡ��ǰ�� PSC (Ԥ��Ƶֵ)
    // ������ CubeMX ������ PSC Ϊ 30 - 1������������� PSC + 1 ���� 30
    uint32_t PSC = htim6.Instance->PSC + 1U;

    // 4. ���� TIM6 ������ʵ�ʵ�����Ƶ�� (���� 90MHz / 30 = 3MHz)
    uint32_t cntFreq = TIM6BaseCLK / PSC;

    // 5. ��Ŀ�� ARR (�� uint64_t ��ֹ 3,000,000 * u_periodMs �м�˷����)
    // ��ʽ: ARR = (cntFreq * periodMs) / (DAC_LUT_LENGTH * 1000) - 1
    targetARR = ((uint64_t)cntFreq * periodMs) / (DAC_LUT_LENGTH * 1000U) - 1U;

    // 6. 16 λ��ʱ���߽籣�� (��ֹ ARR ���� 65535)
    if (targetARR > 65535U) {
        targetARR = 65535U;
    }

    // 7. ֱ��д�� ARR �Ĵ��� (Preload �ѿ�����������һ�������¼��޷���Ч)
    __HAL_TIM_SET_AUTORELOAD(&htim6, (uint32_t) targetARR);
}

void DAC_wave_compute(uint16_t* p_buffer, e_waveType_t e_waveMode, uint32_t FSR) {
	// ÿ�β����Ļ���ֵ
	float step = PI_2 / (float)DAC_LUT_LENGTH; 
	float val = 0.0f;
	float FSRRatio = FSR / 3300.0f;

	switch(e_waveMode) {
		case SINUSOIDAL: // ���Ҳ�
			for(uint32_t i = 0; i < DAC_LUT_LENGTH; i++) {
				val = sinf(step * i);
				// sinf ��Χ -1 ~ +1��̧����ѹ�⵽ 0 ~ 4095 
				p_buffer[i] = (uint16_t)(2047.5f * (val + 1.0f) * FSRRatio); 
			}
			break;
				
		case TRIANGULAR: // ���ǲ�
			for(uint32_t i = 0; i < DAC_LUT_LENGTH; i++) {
				// �ָ�ǰ�����ڣ��������ͺ�����ڣ����䣩 
				if ((step * i) < PI_1) {
						// 0 �� PI ӳ��Ϊ 0.0f �� 1.0f
						val = (step * i) / PI_1; 
				} 
				else {
						// PI �� 2*PI ӳ��Ϊ 1.0f �� 0.0f
						val = 2.0f - ((step * i) / PI_1); 
				}
				p_buffer[i] = (uint16_t)(4095.0f * val * FSRRatio); 
			}
			break;
				
		case SAWTOOTH: // ��ݲ�
			for(uint32_t i = 0; i < DAC_LUT_LENGTH; i++) {
				// current_phase ��Χ 0 ~ 2*PI������ӳ�䵽 0 ~ 4095
				p_buffer[i] = (uint16_t)(4095.0f * ((step * i) / PI_2) * FSRRatio); 
			}
			break;
				
		default:
				for(uint32_t i = 0; i < DAC_LUT_LENGTH; i++) {
					p_buffer[i] = 2048; // �쳣Ĭ����� 1.65V �м�ֵ
				}
				break;
	}
}

static void DAC_restart_dma(void) {
      HAL_TIM_Base_Stop(&htim6);
      HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
      HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)s_DACLUT[s_waveMode], DAC_LUT_LENGTH, DAC_ALIGN_12B_R);
      __HAL_TIM_SET_COUNTER(&htim6, 0);
      HAL_TIM_Base_Start(&htim6);
}

void DAC_set_FSR(int16_t adjFSR) {
	if (s_currFSR != adjFSR) {
		s_currFSR = adjFSR;
		DAC_wave_compute(s_DACLUT[s_waveMode], s_waveMode, s_currFSR);
	}
}

void DAC_set_period(int16_t adjPeriod) {
	if (s_currPeriod != adjPeriod) {
		s_currPeriod = adjPeriod;
		DAC_set_waveform_period_ms(s_currPeriod);
		DAC_restart_dma();
	}
}

void DAC_set_wave(e_waveType_t waveType) {
	if (s_waveMode != waveType) {
		s_waveMode = waveType;
		DAC_wave_compute(s_DACLUT[s_waveMode], s_waveMode, s_currFSR);
		DAC_restart_dma();
	}
}

uint32_t DAC_get_ARR(void) {
	return (uint32_t)targetARR;
}

e_waveType_t DAC_get_waveType(void) {
	return s_waveMode;
}

void DAC_init(void) {
	DAC_wave_compute(s_DACLUT[s_waveMode], s_waveMode, s_currFSR);
	HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)s_DACLUT[SINUSOIDAL], DAC_LUT_LENGTH, DAC_ALIGN_12B_R);
	DAC_set_waveform_period_ms(s_currPeriod);
	HAL_TIM_Base_Start(&htim6);
	
}