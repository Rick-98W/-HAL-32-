#include <ebtn.h>
#include <LED_task.h> // 引用 LED 控制接口
#include <button_task.h> 
#include <USART_task.h>
#include <menu_item.h>

/* --- 1. 按键 ID 定义 --- */
// 这里的 ID 是给 ebtn 库用的，我们自己在回调里区分
enum {
    BTN_1 = 0, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6,
    BTN_WKUP // 作为 Shift 键
};

/* --- 2. 硬件适配层 (修正点：参数必须是 ebtn_btn_t 指针) --- */
// ebtn 库要求这个函数返回 1 代表“按下(Active)”，0 代表“松开”
uint8_t user_btn_read(ebtn_btn_t *btn)
{
	// 从结构体里取出 key_id，知道是要读哪个引脚
	uint16_t btn_id = btn->key_id;
	GPIO_PinState state = GPIO_PIN_SET;

	switch(btn_id) {
			// 根据原理图 KEY1-KEY6是按下接地 (Active Low)
			case BTN_1: state = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin); break;
			case BTN_2: state = HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin); break;
			case BTN_3: state = HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin); break;
			case BTN_4: state = HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin); break;
			case BTN_5: state = HAL_GPIO_ReadPin(KEY5_GPIO_Port, KEY5_Pin); break;
			case BTN_6: state = HAL_GPIO_ReadPin(KEY6_GPIO_Port, KEY6_Pin); break;
			
			// 根据原理图 WKUP也是按下接地 (Active Low)
			case BTN_WKUP: state = HAL_GPIO_ReadPin(WKUP_GPIO_Port, WKUP_Pin); break; 
			
			default: return 0;
	}
	
	// 逻辑转换：如果读到 0 (RESET)，说明按下了，返回 1 给 ebtn
	return (state == GPIO_PIN_RESET) ? 1 : 0;
}

/* --- 3. 参数配置 --- */
// 20ms消抖, 50ms最短点击, 500ms算长按
static const ebtn_btn_param_t btn_param = EBTN_PARAMS_INIT(20, 20, 50, 500, 200, 1000, 10);

/* --- 4. 定义按键对象数组 --- */
static ebtn_btn_t my_btns[] = {
	EBTN_BUTTON_INIT(BTN_1, &btn_param),
	EBTN_BUTTON_INIT(BTN_2, &btn_param),
	EBTN_BUTTON_INIT(BTN_3, &btn_param),
	EBTN_BUTTON_INIT(BTN_4, &btn_param),
	EBTN_BUTTON_INIT(BTN_5, &btn_param),
	EBTN_BUTTON_INIT(BTN_6, &btn_param),
	EBTN_BUTTON_INIT(BTN_WKUP, &btn_param),
};

void button_event_callback(ebtn_btn_t *btn, ebtn_evt_t evt){
	if (evt != EBTN_EVT_ONCLICK) {
			return;
	}
	
	if (menu_is_in_LED_toggle()) {
		switch (btn->key_id) {
			case BTN_1:
					menu_LED_toggle_phase_wrapper(0);
					break;

			case BTN_2:
					menu_LED_toggle_phase_wrapper(1);
					break;

			case BTN_3:
					menu_LED_toggle_phase_wrapper(2);
					break;

			case BTN_4:
					menu_LED_toggle_phase_wrapper(3);
					break;

			case BTN_5:
					menu_LED_toggle_phase_wrapper(4);
					break;

			case BTN_6:
					menu_LED_toggle_phase_wrapper(5);
					break;

			case BTN_WKUP:
					menu_LED_toggle_exit();
					break;

			default:
					break;
		}
	}
	else {
		switch (btn->key_id) {
				case BTN_1:
						menu_navigate(MENU_KEY_UP);
						break;

				case BTN_2:
						menu_navigate(MENU_KEY_DOWN);
						break;

				case BTN_3:
						menu_navigate(MENU_KEY_ENTER);
						break;

				case BTN_4:
						menu_navigate(MENU_KEY_BACK);
						break;

				default:
						break;
		}
	}
	USART_printf("key=%d\r\n", btn->key_id);
}

///* --- 5. 业务逻辑回调 --- */
//void button_event_callback(ebtn_btn_t *btn, ebtn_evt_t evt)
//{
//    // 我们只关心“单击”事件
//    if (evt != EBTN_EVT_ONCLICK) return; 

//    uint16_t id = btn->key_id;
//    if (id == BTN_WKUP) return; // Shift 键本身不触发动作

//    // [修正点] 使用 ebtn_is_btn_active 检查 Shift 键是否处于激活(按下)状态
//    // 这个函数在 ebtn.h 里声明了，可以直接用
//    uint8_t is_shift = ebtn_is_btn_active(&my_btns[BTN_WKUP]);

//    if (is_shift) {
//        // === 组合操作 (按住 WKUP + Key) ===
//        switch(id) {
//            case BTN_2: LED_save_state();       break; // 存档
//            case BTN_3: LED_restore_state();    break; // 读档
//            case BTN_4: LED_set_blackout(1);    break; // 全灭
//            case BTN_5: LED_set_blackout(0);    break; // 恢复
//            default: break;
//        }
//    } else {
//        // === 普通操作 (直接点击 Key) ===
//			 // 切换对应 LED 的相位

//            LED_toggle_phase(id);
//						USART_printf("Key%d pressed\r\n",id + 1);
//    }
//}

/* --- 6. 任务接口 (给 Scheduler 用) --- */
void button_task_init(void) {
    // 注册按键，注意这里传入的是 user_btn_read，它的签名现在对了
    ebtn_init(my_btns, 
              sizeof(my_btns) / sizeof(ebtn_btn_t), 
              NULL, 0, 
              user_btn_read, 
              button_event_callback);
}

void button_proc(void) {
    ebtn_process(HAL_GetTick());
}