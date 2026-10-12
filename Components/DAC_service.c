#include <DAC_service.h>
#include "dac.h"
#include "tim.h"
#include "math.h"

#define PI_2 (6.2831853f)
#define PI_1 (3.1415926f)
#define DAC_LUT_LENGTH 250U

static uint16_t s_DACLUT[3][DAC_LUT_LENGTH];
uint32_t FSR = 3300;

typedef enum {
  SINUSOIDAL = 0,
  TRIANGULAR,
  SAWTOOTH
} e_waveType_t;

static e_waveType_t s_waveMode = SINUSOIDAL;

/**
 * @brief  动态设置 DAC 输出波形的周期
 * @param  u_periodMs 目标波形周期，单位毫秒 (ms)
 */
void DAC_set_waveform_period_ms(uint32_t periodMs) {
    if (periodMs == 0) return;

    // 1. 获取 APB1 总线时钟
    uint32_t PCLK1 = HAL_RCC_GetPCLK1Freq();
    uint32_t TIM6BaseCLK;

    // 2. 根据 APB1 预分频系数计算 TIM6 的输入时钟 (分频系数 != 1 时，定时器时钟自动 x2，即 90MHz)
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        TIM6BaseCLK = PCLK1 * 2U;
    }
    else {
        TIM6BaseCLK = PCLK1;
    }

    // 3. 从 htim6 句柄直接读取当前的 PSC (预分频值)
    // 假设在 CubeMX 配置了 PSC 为 30 - 1，这里读出来的 PSC + 1 就是 30
    uint32_t PSC = htim6.Instance->PSC + 1U;

    // 4. 计算 TIM6 计数器实际的运行频率 (例如 90MHz / 30 = 3MHz)
    uint32_t cntFreq = TIM6BaseCLK / PSC;

    // 5. 算目标 ARR (用 uint64_t 防止 3,000,000 * u_periodMs 中间乘法溢出)
    // 公式: ARR = (cntFreq * periodMs) / (DAC_LUT_LENGTH * 1000) - 1
    uint64_t targetARR = ((uint64_t)cntFreq * periodMs) / (DAC_LUT_LENGTH * 1000U) - 1U;

    // 6. 16 位定时器边界保护 (防止 ARR 超过 65535)
    if (targetARR > 65535U) {
        targetARR = 65535U;
    }

    // 7. 直接写入 ARR 寄存器 (Preload 已开启，将在下一个更新事件无缝生效)
    __HAL_TIM_SET_AUTORELOAD(&htim6, (uint32_t) targetARR);
}

void DAC_wave_compute(uint16_t* p_buffer, e_waveType_t e_waveMode) {
    // 每次步进的弧度值
    float step = PI_2 / (float)DAC_LUT_LENGTH; 
    float val = 0.0f;

		switch(e_waveMode) {
			case SINUSOIDAL: // 正弦波
				for(uint32_t i = 0; i < DAC_LUT_LENGTH; i++) {
					val = sinf(step * i);
					// sinf 范围 -1 ~ +1，抬升并压扁到 0 ~ 4095 
					p_buffer[i] = (uint16_t)(2047.5f * (val + 1.0f)); 
				}
				break;
					
			case TRIANGULAR: // 三角波
				for(uint32_t i = 0; i < DAC_LUT_LENGTH; i++) {
					// 分割前半周期（爬升）和后半周期（回落） 
					if ((step * i) < PI_1) {
							// 0 到 PI 映射为 0.0f 到 1.0f
							val = (step * i) / PI_1; 
					} 
					else {
							// PI 到 2*PI 映射为 1.0f 到 0.0f
							val = 2.0f - ((step * i) / PI_1); 
					}
					p_buffer[i] = (uint16_t)(4095.0f * val); 
				}
				break;
					
			case SAWTOOTH: // 锯齿波
				for(uint32_t i = 0; i < DAC_LUT_LENGTH; i++) {
					// current_phase 范围 0 ~ 2*PI，线性映射到 0 ~ 4095
					p_buffer[i] = (uint16_t)(4095.0f * ((step * i) / PI_2)); 
				}
				break;
					
			default:
					for(uint32_t i = 0; i < DAC_LUT_LENGTH; i++) {
						p_buffer[i] = 2048; // 异常默认输出 1.65V 中间值
					}
					break;
		}
}

void DAC_init(void) {
	DAC_wave_compute(s_DACLUT[SINUSOIDAL],SINUSOIDAL);
	HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)s_DACLUT[SINUSOIDAL], DAC_LUT_LENGTH, DAC_ALIGN_12B_R);
	DAC_set_waveform_period_ms(5000);
	HAL_TIM_Base_Start(&htim6);
	
}