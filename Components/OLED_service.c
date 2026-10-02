#include <OLED_service.h>
#include "i2c.h"
#include "stdio.h"
#include "stdarg.h"
#include "OLED.h"
#include "u8g2.h"

static uint8_t  s_oled_refresh_pending = 0;
static uint32_t s_oled_refresh_due = 0;

static u8g2_t I2C_SSD_091;

static OLED_render_cb_t sfn_renderCallback = NULL;

void OLED_service_register_render_cb(OLED_render_cb_t fn_render) {
	if (fn_render != NULL) {
		sfn_renderCallback = fn_render;
	}
}

// u8g2 的 GPIO 和延时回调函数
uint8_t u8g2_gpio_and_delay_STM32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  switch(msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      // 初始化 GPIO (如果需要，例如 SPI 的 CS, DC, RST 引脚)
      // 对于硬件 I2C，这里通常不需要做什么
      break;
    case U8X8_MSG_DELAY_MILLI:
      // 原因: u8g2 内部某些操作需要毫秒级的延时等待。
      // 提供毫秒级延时，直接调用 HAL 库函数。
      HAL_Delay(arg_int - 1);
      break;
    case U8X8_MSG_DELAY_10MICRO:
      // 原因: 某些通信协议或显示时序可能需要微秒级延时。
      // 提供 10 微秒延时。HAL_Delay(1) 精度不够（通常是毫秒级）。
      // 需要更精确的延时，可以使用 CPU NOP 指令或 DWT 定时器。
      // 以下简单循环仅为示例，**需要根据你的 CPU 时钟频率精确调整循环次数**。
      for (volatile uint32_t i = 0; i < 150; ++i) {} // 示例循环，计数需调整
      break;
    case U8X8_MSG_DELAY_100NANO:
      // 原因: 更精密的时序控制，通常在高速接口或特定操作中需要。
      // 提供 100 纳秒延时。非常短，通常用 NOP 指令实现。
      // **同样需要根据 CPU 时钟频率调整 NOP 数量**。
       __NOP(); __NOP(); __NOP(); // 示例 NOP
      break;
    case U8X8_MSG_GPIO_I2C_CLOCK: // [[fallthrough]] // Fallthrough 注释表示有意为之
    case U8X8_MSG_GPIO_I2C_DATA:
      // 控制 SCL/SDA 引脚电平。这些仅在**软件模拟 I2C** 时需要实现。
      // 使用硬件 I2C 时，这些消息可以忽略，由 HAL 库处理。
      break;
     // --- 以下是 GPIO 相关的消息，主要用于按键输入或 SPI 控制 --- 
     // 如果你的 u8g2 应用需要读取按键或控制 SPI 引脚 (CS, DC, Reset)，
     // 你需要在这里根据 msg 类型读取/设置对应的 GPIO 引脚状态。
     // 对于仅使用硬件 I2C 显示的场景，可以像下面这样简单返回不支持。
     case U8X8_MSG_GPIO_CS:
        // SPI 片选控制
        break;
      case U8X8_MSG_GPIO_DC:
        // SPI 数据/命令线控制
        break;
      case U8X8_MSG_GPIO_RESET:
        // 显示屏复位引脚控制
        break;
    case U8X8_MSG_GPIO_MENU_SELECT:
      u8x8_SetGPIOResult(u8x8, /* 读取选择键 GPIO 状态 */ 0);
      break;
    default:
      u8x8_SetGPIOResult(u8x8, 1); // 不支持的消息
      break;
  }
  return 1;
}

// u8g2 的硬件 I2C 通信回调函数
uint8_t u8x8_byte_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  static uint8_t buffer[32]; // u8g2 每次传输最大 32 字节
  static uint8_t buf_idx;
  uint8_t *data;

  switch(msg) {
    case U8X8_MSG_BYTE_SEND:
      // 原因: u8g2 通常不会一次性发送大量数据，而是分块发送。
      // 这个消息用于将一小块数据 (arg_int 字节) 从 u8g2 内部传递到我们的回调函数。
      // 我们需要将这些数据暂存到本地 buffer 中，等待 START/END_TRANSFER 信号。
      data = (uint8_t *)arg_ptr;
      while( arg_int > 0 ) {
        buffer[buf_idx++] = *data;
        data++;
        arg_int--;
      }
      break;
    case U8X8_MSG_BYTE_INIT:
      // 原因: 提供一个机会进行 I2C 外设的初始化。
      // 初始化 I2C (通常在 main 函数中已完成)
      // 由于我们在 main 函数中已经调用了 MX_I2C1_Init()，这里通常可以留空。
      break;
    case U8X8_MSG_BYTE_SET_DC:
      // 原因: 这个消息用于 SPI 通信中控制 Data/Command 选择引脚。
      // 设置数据/命令线 (I2C 不需要)
      // I2C 通过特定的控制字节 (0x00 或 0x40) 区分命令和数据，因此该消息对于 I2C 无意义。
      break;
    case U8X8_MSG_BYTE_START_TRANSFER:
      // 原因: 标记一个 I2C 传输序列的开始。
      buf_idx = 0;
      // 我们在这里重置本地缓冲区的索引，准备接收新的数据块。
      break;
    case U8X8_MSG_BYTE_END_TRANSFER:
      // 原因: 标记一个 I2C 传输序列的结束。
      // 此时，本地 buffer 中已经暂存了所有需要发送的数据块。
      // 这是执行实际 I2C 发送操作的最佳时机。
      // 发送缓冲区中的数据
      // 注意: u8x8_GetI2CAddress(u8x8) 返回的是 7 位地址 * 2 = 8 位地址
      if (HAL_I2C_Master_Transmit(&hi2c1, u8x8_GetI2CAddress(u8x8), buffer, buf_idx, 100) != HAL_OK) {
        return 0; // 发送失败
      }
      break;
    default:
      return 0;
  }
  return 1;
}

void OLED_request_refresh(uint32_t delay_ms) {
	s_oled_refresh_due = HAL_GetTick() + delay_ms;
	s_oled_refresh_pending = 1;
}

void OLED_task_poll(void) {
	if (!s_oled_refresh_pending) {
		return;
	}

	if ((int32_t)(HAL_GetTick() - s_oled_refresh_due) < 0) {
		return;
	}

	// 先清除，保证本次只触发一次
	s_oled_refresh_pending = 0;

	OLED_proc();
}

void OLED_init() {
	OLED_Init();
	
		// --- U8G2 初始化 ---
	// 1. Setup: 这是最关键的一步，它配置了 u8g2 实例。
  //    - 选择与硬件匹配的 setup 函数 (SSD1306, I2C, 128x32, Full Buffer)。
  //    - &u8g2: 指向要配置的 u8g2 结构体实例的指针。
  //    - U8G2_R0: 旋转设置。U8G2_R0=0°, U8G2_R1=90°, U8G2_R2=180°, U8G2_R3=270°。
  //    - u8x8_byte_hw_i2c: 指向你的硬件 I2C 字节传输回调函数的指针。
  //    - u8g2_gpio_and_delay_stm32: 指向你的 GPIO 和延时回调函数的指针。
	u8g2_Setup_ssd1306_i2c_128x32_univision_f(&I2C_SSD_091, U8G2_R0, u8x8_byte_hw_i2c, u8g2_gpio_and_delay_STM32);
  // 2. Init Display: 发送初始化序列到 OLED
  u8g2_InitDisplay(&I2C_SSD_091);
  // 3. Set Power Save: 唤醒屏幕。
  //    - 参数 0 表示关闭省电模式 (屏幕亮起)。
  //    - 参数 1 表示进入省电模式 (屏幕熄灭)。
  u8g2_SetPowerSave(&I2C_SSD_091, 0);
  // 4. 清空U8G2的buffer
	u8g2_ClearBuffer(&I2C_SSD_091);
	// 5. 设置字体
	u8g2_SetFont(&I2C_SSD_091, u8g2_font_ncenB08_tr);
	u8g2_DrawStr(&I2C_SSD_091, 0, 8, "GD32HAL DevPlatform");
	u8g2_DrawButtonUTF8(&I2C_SSD_091, 26, 26, U8G2_BTN_BW1 | U8G2_BTN_SHADOW1, 0, 0, 0, "Powered by Rick");
	u8g2_SendBuffer(&I2C_SSD_091);
	OLED_request_refresh(1500);
	
}

void OLED_printf(uint8_t x, uint8_t y, uint8_t fontsize, const char *fmt, ...) {
	char temp_buf[24]; 
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(temp_buf, sizeof(temp_buf), fmt, args);
	va_end(args);

	if (len <= 0) return;

	OLED_ShowStr(x, y, temp_buf, fontsize);
}

void *OLED_service_get_u8g2_handle(void) {
    return (void *)&I2C_SSD_091;
}



void OLED_proc(){
	if (sfn_renderCallback != NULL) {
		sfn_renderCallback();
	}
}
