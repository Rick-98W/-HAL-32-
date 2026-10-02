#include <LED_task.h>
#include <USART_task.h>

#include "math.h"
#include "tim.h"

#define PI 3.14159f

static uint8_t s_ucLED[4] = {1};
void LED_disp(uint8_t *s_ucLED);


/* --- 1. 变量搬家：从函数内部提出来，方便控制 --- */
static uint32_t s_breathCounter = 0;       // 时间轴
static uint32_t s_breathCheckpoint = 0;   // 存档点
static uint8_t  s_pwmCounter = 0;          // PWM 计数
static uint8_t  s_blackout = 0;         // 全灭开关
static float    s_LEDPhase[6] = {0};       // 相位偏移数组 (0 或 1)
static uint8_t  s_brightness;

// 你的参数配置 (保持你提供的数值)
static const uint32_t k_breathPeriod = 4000; 
static const uint8_t  k_pwmMax = 8;
static const float phaseShift = PI / 3.0f;

//TIM呼吸灯变量---------------------------------------
#define LED_LUT_LENGTH 4000U

typedef enum{
    LED_BUILDING,
    LED_START_PENDING,
    LED_RUNNING
}LED_State_t;

typedef enum{
	LED_DMA_CIRCULAR,
	LED_DMA_NORMAL
}LED_DMA_State_t;

static LED_State_t s_LEDState = LED_BUILDING;
	
static uint32_t s_tableLength = LED_LUT_LENGTH;
static uint16_t s_LEDLUT[LED_LUT_LENGTH][2];
static uint8_t  s_tableReady = 0;
static uint8_t  s_waveWidth = 8;
static uint32_t s_DMACounter;

volatile static LED_DMA_State_t s_DMAState = LED_DMA_NORMAL;

/* --- 2. 核心处理函数 --- */
void LED_soft_breathe(void)
{
    // 如果全灭模式开启，强制关闭所有灯
    if (s_blackout) {
        for(int i=0; i<4; i++) s_ucLED[i] = 0;
        LED_disp(s_ucLED);
        // s_breathCounter 依然在走，时间在流逝，只是看不见
        s_breathCounter = (s_breathCounter + 1) % k_breathPeriod;
        return; 
    }

    // 正常的呼吸逻辑

    s_breathCounter = (s_breathCounter + 1) % k_breathPeriod;
    s_pwmCounter = (s_pwmCounter + 1) % k_pwmMax;

    for(uint8_t i = 0; i < 4; i++) 
    {
        // === 修改点：加入 s_LEDPhase[i] 实现相位翻转 ===
        // 乘以 PI，如果是 1.0f 就是加 π (倒立)，如果是 0.0f 就是不变
        float angle = (2.0f * PI * s_breathCounter) / k_breathPeriod + i * phaseShift + (s_LEDPhase[i] * PI);

        float sinValue = sinf(angle);
        
        // 开方增强 (powf 0.5)
        float enhancedValue = sinValue > 0 ? sqrtf(sinValue) : -sqrtf(-sinValue);

        // 压扁抬升
        s_brightness = (uint8_t)((enhancedValue + 1.0f) * k_pwmMax / 2.0f);
        
        s_ucLED[i] = (s_pwmCounter < s_brightness) ? 1 : 0;
			
			
//			s_ucLED[i] = s_LEDPhase[i];
			
    }

    LED_disp(s_ucLED);
}

void LED_disp(uint8_t *s_ucLED)
{
	  uint8_t temp = 0x00;                // 用于记录当前 LED 状态的临时变量 (最低4位有效)
    static uint8_t temp_pre = 0xff;     // 记录之前 LED 状态的变量, 用于判断是否需要更新显示

    for (int i = 0; i < 4; i++)         // 遍历4个LED的状态
    {
        // 将LED状态整合到temp变量中，方便后续比较
        if (s_ucLED[i]) temp |= (1 << i); // 如果s_ucLED[i]为1, 则将temp的第i位置1
    }

    // 仅当当前状态与之前状态不同的时候，才更新显示
    if (temp != temp_pre)
    {
        // 使用HAL库函数根据temp的值设置对应引脚状态 (假设低电平点亮)
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, (temp & 0x01) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, (temp & 0x02) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, (temp & 0x04) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, (temp & 0x08) ? GPIO_PIN_RESET : GPIO_PIN_SET);

        temp_pre = temp;                // 更新记录的旧状态
    }
}

//TIM呼吸灯函数----------------------------------------------------------------------

//计算正弦点数
static uint16_t calc_LED_LUT(uint32_t pointIdx, uint32_t domain, uint32_t range, float phaseoffset, uint8_t toggle){
	float angle = (2.0f * PI * pointIdx) / domain + phaseoffset + toggle * PI;
	float sinValue = sinf(angle);
	// 开方增强 (powf 0.5)
	float enhancedValue = sinValue > 0 ? sqrtf(sinValue) : -sqrtf(-sinValue);
	// 压扁抬升
	uint16_t pointVal = (uint16_t)((enhancedValue + 1.0f) * range / 2.0f);
	return pointVal;
}

//LUT计算
void LED_build_LUT(uint32_t index, float offset){
	for (uint32_t i = 0; i < LED_LUT_LENGTH; i++){
		s_LEDLUT[i][index - 5] = calc_LED_LUT(i, s_tableLength, s_waveWidth, offset, s_LEDPhase[index - 1]);
	}
}

//装配启动
static HAL_StatusTypeDef LED_hardware_start(void)
{
	HAL_StatusTypeDef status;
	if (s_tableReady == 0) return 9;

	HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_2);

	//CCR的首次装载
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, s_LEDLUT[0][0]);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, s_LEDLUT[0][1]);
	//preload 装载到 active CCR
	HAL_TIM_GenerateEvent(&htim4, TIM_EVENTSOURCE_UPDATE);

	//配置并启动 TIM4 Update DMA burst
	status = HAL_TIM_DMABurst_MultiWriteStart(&htim4, TIM_DMABASE_CCR1, TIM_DMA_UPDATE, (uint32_t *)s_LEDLUT,
																		TIM_DMABURSTLENGTH_2TRANSFERS, LED_LUT_LENGTH * 2U);

	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);

	HAL_NVIC_DisableIRQ(DMA1_Stream6_IRQn);
	
	s_LEDState = LED_RUNNING;
	s_DMAState = LED_DMA_CIRCULAR;
	
	return status;
}

//重定义DMA multi-Burst的ISR
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	if (htim == &htim4){
		if (s_DMAState == LED_DMA_NORMAL){ //当DMA模式为normal
			HAL_TIM_DMABurst_WriteStop(&htim4, TIM_DMA_UPDATE);
			__HAL_DMA_DISABLE(htim4.hdma[TIM_DMA_ID_UPDATE]);
			// 等待硬件完全清零 EN 位 (防止总线锁死)
			while ((htim4.hdma[TIM_DMA_ID_UPDATE]->Instance->CR & DMA_SxCR_EN) != 0U);
			// 将DMA改回circular
			htim4.hdma[TIM_DMA_ID_UPDATE]->Instance->CR |= DMA_SxCR_CIRC;
//			LED_hardware_start();
			LED_hardware_start();
		}
	}
}

//调度器轮询任务接口
void LED_proc(void){
	switch (s_LEDState)
	{
			case LED_BUILDING:
				LED_build_LUT(5, phaseShift * 4);
				LED_build_LUT(6, phaseShift * 5);
				s_tableReady = 1;
				s_LEDState = LED_START_PENDING;
				break;

			case LED_START_PENDING:
				LED_hardware_start();
				s_LEDState = LED_RUNNING;
				break;

			case LED_RUNNING:
				LED_soft_breathe();
				break;
	}
}


/* --- 3. 控制接口实现 (供按键与串口调用) --- */

// 翻转相位：在 0.0 和 1.0 之间切换
void LED_toggle_phase(uint8_t LEDIndex) {
	if (LEDIndex < 6) {
			s_LEDPhase[LEDIndex] = (s_LEDPhase[LEDIndex] == 0.0f) ? 1.0f : 0.0f;
	}
	if (LEDIndex == 4 || LEDIndex == 5) {
			LED_build_LUT(LEDIndex + 1, phaseShift * LEDIndex);
	}
}

// 存档：记录当前时间点
void LED_save_state(void) {
	s_breathCheckpoint = s_breathCounter;
	s_DMACounter = __HAL_DMA_GET_COUNTER(htim4.hdma[TIM_DMA_ID_UPDATE]);
}

// 读档：让时间倒流回存档点
void LED_restore_state(void) {
	// 如果当前是Normal模式，完全重置到Circular状态
	if (s_DMAState == LED_DMA_NORMAL) {
		// 1. 完全停止TIM4 PWM和DMA
		HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1);
		HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_2);
		HAL_TIM_DMABurst_WriteStop(&htim4, TIM_DMA_UPDATE);

		// 2. 禁用DMA并等待
		__HAL_DMA_DISABLE(htim4.hdma[TIM_DMA_ID_UPDATE]);
		uint32_t timeout = 100000;
		while ((htim4.hdma[TIM_DMA_ID_UPDATE]->Instance->CR & DMA_SxCR_EN) != 0U) {
			if (--timeout == 0) return;  // 超时则放弃本次restore
		}

		// 3. 清除DMA标志位
		__HAL_DMA_CLEAR_FLAG(htim4.hdma[TIM_DMA_ID_UPDATE], __HAL_DMA_GET_TC_FLAG_INDEX(htim4.hdma[TIM_DMA_ID_UPDATE]));

		// 4. 恢复Circular模式
		htim4.hdma[TIM_DMA_ID_UPDATE]->Instance->CR |= DMA_SxCR_CIRC;

		// 5. 重新启动并恢复为circular模式
		LED_hardware_start();

		// 现在处于安全的Circular状态
	}

	// 执行正常的restore流程
	s_breathCounter = s_breathCheckpoint;
	__HAL_DMA_DISABLE(htim4.hdma[TIM_DMA_ID_UPDATE]);
	// 等待硬件完全清零 EN 位 (防止总线锁死)
	while ((htim4.hdma[TIM_DMA_ID_UPDATE]->Instance->CR & DMA_SxCR_EN) != 0U);

	uint32_t totalTransfers = LED_LUT_LENGTH * 2U;
	// 算出存档时已经搬运的数据个数
	uint32_t transferred_count = totalTransfers - s_DMACounter;
	// 更新 M0AR：基地址 + 已搬运偏移量 (因为 s_LEDLUT 是 uint16_t 类型，地址会自动乘 2 字节)
	htim4.hdma[TIM_DMA_ID_UPDATE]->Instance->M0AR = (uint32_t)((uint16_t *)s_LEDLUT + transferred_count);
	__HAL_DMA_SET_COUNTER(htim4.hdma[TIM_DMA_ID_UPDATE], s_DMACounter);
	// 清除 DMA 可能残留的标志位
	__HAL_DMA_CLEAR_FLAG(htim4.hdma[TIM_DMA_ID_UPDATE], __HAL_DMA_GET_TC_FLAG_INDEX(htim4.hdma[TIM_DMA_ID_UPDATE]));
	// 改为normal模式并打开中断响应重启DMA
	htim4.hdma[TIM_DMA_ID_UPDATE]->Instance->CR &= ~DMA_SxCR_CIRC;
	HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
	__HAL_DMA_ENABLE_IT(htim4.hdma[TIM_DMA_ID_UPDATE], DMA_IT_TC);
	__HAL_DMA_ENABLE(htim4.hdma[TIM_DMA_ID_UPDATE]);
	//DMA模式标志位置为normal
	s_DMAState = LED_DMA_NORMAL;
}

// 全灭控制
void LED_set_blackout(uint8_t enable) {
	s_blackout = enable;
	if (enable) {
		// 清零 CCMR1 中 CH1(Bit 6:4) 与 CH2(Bit 14:12) 的 OCxM 位，并置位为 Force Active (0x5)
		TIM4->CCMR1 = (TIM4->CCMR1 & ~((0x7U << 4) | (0x7U << 12))) 
									| ((0x4U << 4) | (0x4U << 12));
	}
	else {
		// 清零并恢复为 PWM Mode 1 (0x6)
		TIM4->CCMR1 = (TIM4->CCMR1 & ~((0x7U << 4) | (0x7U << 12))) 
									| ((0x6U << 4) | (0x6U << 12));
	}
}

// [Get] 获取当前呼吸的“总周期”
uint32_t LED_Get_Period(void) {
    return k_breathPeriod; // 返回那个 4000
}

// [Get] 获取当前呼吸到了“第几步” (0 ~ 4000)
uint32_t LED_Get_Counter(void) {
    return s_breathCounter;
}

// [New] 获取当前 LED 的相位偏移量 (0.0 ~ 1.0)
// 0.0 代表 0度, 0.5 代表 180度, 1.0 代表 360度
float LED_get_phaseOffset(uint32_t index) {
    if (index >= 6) return 0.0f;

    // 1. 基础相位: i / 6.0
    // 注意要写 4.0f 强制转浮点，不然整数除法 i/6 直接变0了
    float basePhase = (float)index / 6.0f;

    // 2. 翻转相位: 如果 s_LEDPhase 是 1.0 (代表π)，那就是 +0.5 (代表180度)
    // 原理：s_LEDPhase[i] * PI / (2 * PI) = s_LEDPhase[i] * 0.5
    float toggleOffset = (s_LEDPhase[index] > 0.5f) ? 0.5f : 0.0f;

    // 3. 归一化 (相当于 % 1.0)
    float total = (float) s_breathCounter / (float) k_breathPeriod + basePhase + toggleOffset;
    while (total >= 1.0f) total -= 1.0f;
    
    return total;
}

uint8_t Get_brightness(){
	return s_brightness;
}

uint8_t Get_Counter(){
	return s_pwmCounter;
}
