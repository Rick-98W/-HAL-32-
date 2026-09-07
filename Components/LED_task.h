#ifndef LED_TASK_H
#define LED_TASK_H

#include "main.h"

// 核心处理函数
void LED_proc(void);

// === 新增：控制接口 ===
void LED_toggle_phase(uint32_t LEDIndex); // 翻转相位
void LED_save_state(void);                // 存档 (Shift + K2)
void LED_restore_state(void);             // 读档 (Shift + K3)
void LED_set_blackout(uint8_t enable);    // 全灭控制 (Shift + K4/K5

float LED_get_phaseOffset(uint32_t i);


uint8_t Get_Brightness();
uint8_t Get_Counter();

#endif