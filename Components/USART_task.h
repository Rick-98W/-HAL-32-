#ifndef USART_TASK_H
#define USART_TASK_H

#include "main.h"

/* --- Public API --- */

/**
 * @brief  初始化通信模块 (启动 DMA 接收)
 * @note   需要在 MX_USARTx_UART_Init() 之后调用
 */
void USART_init(void);

/**
 * @brief  通信主任务处理函数
 * @note   需要在主循环 while(1) 中调用
 */
void USART_proc(void);

/**
 * @brief  通过 DMA 发送格式化日志 (非阻塞，但在 Buffer 忙时会等待)
 * @param  fmt: 格式化字符串 (类似 printf)
 * @param  ...: 可变参数
 * @note   Atomic DMA transaction.
 */
void USART_printf(const char *fmt, ...);


#endif