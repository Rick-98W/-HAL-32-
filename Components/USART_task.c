#include <USART_task.h>
#include "usart.h"
#include <LED_task.h>
#include "ringbuffer.h" // 【新增】引入 RingBuffer
#include "stdio.h"
#include "stdarg.h"
#include "string.h"

/* --- Configuration --- */
#define USART_RX_RING_SIZE   2048
#define USART_RX_LINE_MAX    256     // 单条指令最大长度

#define USART_TX_RING_SIZE   2048    // 【新增】TX RingBuffer 大小 (足够大以缓冲突发日志)
#define USART_TX_DMA_SIZE    512     // 【新增】单次 DMA 搬运的最大长度

/* --- Private Variables --- */

// 1. 接收相关
static struct rt_ringbuffer s_RxRingCtrl;
static uint8_t s_RxRingPool[USART_RX_RING_SIZE];
static uint8_t s_RxDMABuffer[USART_RX_LINE_MAX];       // DMA 写这里

// 传给APP 用的“线性”暂存区 (解决 RingBuffer 回卷问题)
static char s_RxAppBuffer[USART_RX_LINE_MAX];
static uint16_t s_RxLineIdx = 0;                       // 指令拼凑游标

// 2. 发送相关
// RingBuffer 控制块与内存池
static struct rt_ringbuffer s_TxRingCtrl;
static uint8_t s_TxRingPool[USART_TX_RING_SIZE];

// DMA 专用的“线性”暂存区 (解决 RingBuffer 回卷问题)
static uint8_t s_TxDMABuffer[USART_TX_DMA_SIZE];

// 发送状态标志 (1: DMA正在忙, 0: DMA空闲)
static volatile uint8_t b_TxDMABusy = 0;

/* --- Private Function Prototypes --- */
static void USART_parse_command(char *cmd);
static void USART_trigger_TX(void);  //“提醒发货”函数
static uint8_t USART_stream_load(void); //扫描Rx_RingBuffer以抽取command给parse函数

/* --- Public Function Implementation --- */

void USART_init(void)
{
    // --- RX Init ---
		rt_ringbuffer_init(&s_RxRingCtrl, s_RxRingPool, USART_RX_RING_SIZE);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_RxDMABuffer, USART_RX_LINE_MAX);
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

    // --- TX Init ---
    // 初始化 RingBuffer
    rt_ringbuffer_init(&s_TxRingCtrl, s_TxRingPool, USART_TX_RING_SIZE);
    
    // 初始状态 DMA 是闲着的
    b_TxDMABusy = 0;
}

void USART_proc(void)
{
		if (USART_stream_load()){
			USART_parse_command(s_RxAppBuffer);
		}
}

// 改造后的 printf：只管往里塞，塞完喊一声 DMA
void USART_printf(const char *fmt, ...)
{
    // 1. 格式化数据到局部栈空间 (速度极快)
    char temp_buf[256]; 
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(temp_buf, sizeof(temp_buf), fmt, args);
    va_end(args);

    if (len <= 0) return;
		if (len > (int)sizeof(temp_buf) - 1) len = sizeof(temp_buf) - 1;

    // 2. 压入 RingBuffer (Critical Section)
    // 因为 TxCpltCallback 中断里也会操作 RingBuffer (get)，所以这里必须关中断保护 put
    __disable_irq();
    
    // 使用标准 put，满了就丢弃新数据，保证安全性
    rt_ringbuffer_put(&s_TxRingCtrl, (uint8_t *)temp_buf, len);

    // 3. 核心逻辑：检查 DMA 是否在偷懒
    if (b_TxDMABusy == 0)
    {
        // 如果 DMA 闲着，赶紧踹它一脚，让它开始干活
        USART_trigger_TX();
    }
    
    __enable_irq();
}

/* --- Private Function Implementation --- */

/**
 * @brief  “提醒发货”函数 (DMA 调度器)
 * @note   负责从 RingBuffer 搬运数据到线性 Buffer，并启动 DMA
 * 这个函数既会被 printf 调用，也会被中断回调调用
 */
static void USART_trigger_TX(void)
{
    // 1. 再次确认忙闲状态 (虽然调用前通常判断过，但双重保险没错)
    if (b_TxDMABusy) return;

    // 2. 从 RingBuffer 取货
    // rt_ringbuffer_get 会自动处理回卷，并更新 read_index
    // 返回值是实际取到的字节数
    rt_size_t len = rt_ringbuffer_get(&s_TxRingCtrl, s_TxDMABuffer, USART_TX_DMA_SIZE);

    if (len > 0)
    {
        // 3. 有货！标记状态为忙
        b_TxDMABusy = 1;

        // 4. 启动 DMA 发送 (非阻塞)
        // 注意：这里使用的是我们搬运出来的 s_TxDmaBuffer (线性连续内存)
        if (HAL_UART_Transmit_DMA(&huart1, s_TxDMABuffer, len) != HAL_OK)
        {
            // 如果启动失败 (极罕见)，清除忙标志，防止死锁
            b_TxDMABusy = 0;
        }
    }
    else
    {
        // 没货了，DMA 可以休息了
        b_TxDMABusy = 0;
    }
}

static uint8_t USART_stream_load(void)
{
    uint8_t ch;
    
    // 每次处理一个字符
    while(rt_ringbuffer_getchar(&s_RxRingCtrl, &ch)){
        
        // 1. 结束符判断：兼容协议分号和串口工具常用的 CR/LF
        if (ch == ';' || ch == '\n'){
            s_RxAppBuffer[s_RxLineIdx] = '\0'; // 封口
            s_RxLineIdx = 0; // 重置
            return 1;        // 成功！
        }

        // CRLF 的 CR 只作为行结束的一部分，不放进命令字符串
        if (ch == '\r') {
            continue;
        }
        
        // 2. 正常存入（带溢出保护）
        // 留一个位置给 \0，所以是 MAX - 1
        if (s_RxLineIdx < USART_RX_LINE_MAX - 1) {
            s_RxAppBuffer[s_RxLineIdx++] = ch;
        }
        else {
            // 溢出处理：你可以选择清空重来，或者丢弃新数据
            // 这里简单策略：重置，视作上一条指令作废
            s_RxLineIdx = 0; 
        }
    }
    return 0; 
}

// ... (USART_parse_command 保持不变) ...
static void USART_parse_command(char *cmd)
{
    USART_printf("Command Received:%s\r\n",cmd);
		// 标志位：记录这一句话里有没有我们要干的活
		// 如果干了活，置1；如果到最后还是0，说明是废话
		uint8_t isHandled = 0;
		
		char *pToggle = strstr(cmd, "Toggle");
    if (pToggle != NULL)
    {
        isHandled = 1; // 标记：我们干活了
        
        // 跳过 "Toggle" 这6个字母，从后面开始扫荡
        char *pScan = pToggle + 6; 
        uint8_t toggle_cnt = 0;

        while (*pScan != '\0' && *pScan != '\r' && *pScan != '\n')
        {
            // 只要发现是 '1'~'6' 的数字，立刻开火
            if (*pScan >= '1' && *pScan <= '6')
            {
                int id = *pScan - '0';
                LED_toggle_phase(id - 1);
                USART_printf("[CMD] Toggle: LED%d\r\n", id);
                toggle_cnt++;
            }
            pScan++; // 继续往后找，绝不回头
        }
        
        if (toggle_cnt == 0) USART_printf("[WRN] Toggle cmd found, but no ID (1-6)!\r\n");
    }

    // ============================================================
    // 2. 【电源控制模块】: On / Off
    // ============================================================
    // 使用 strstr 允许指令夹在句子里，但要注意 "Only" 这种词包含 "On"
    // 为了防止误判，这里稍微严格一点，或者你如果不介意误判就直接用
    
    // 优先判断 Off (因为 Off 通常权重高，或者处于安全考虑)
    if (strstr(cmd, "Off") != NULL)
    {
        LED_set_blackout(1);
        USART_printf("[SYS] System: OFFLINE (Blackout)\r\n");
        isHandled = 1;
    }
    // 判断 On (且当前没有找到 Off，防止 "Turn On then Turn Off" 产生冲突，
    // 或者你可以去掉 else 让它支持先开再关)
    else if (strstr(cmd, "On") != NULL) 
    {
        LED_set_blackout(0);
        USART_printf("[SYS] System: ONLINE\r\n");
        isHandled = 1;
    }

    // ============================================================
    // 3. 【状态查询模块】: Phase 查询
    // 支持 "Check Phase 1" 或 "LED1Phase"
    // ============================================================
    char *pPhase = strstr(cmd, "Phase");
    if (pPhase != NULL)
    {
        isHandled = 1;
        
        // 策略：在整句话里找数字。
        // 为了避免和 Toggle 的数字混淆，这里可以做得更精细，
        // 但简单起见，我们假设 Phase 也是扫荡整句里的数字
        char *pScan = cmd;
        while (*pScan != '\0' && *pScan != '\r' && *pScan != '\n')
        {
						if (*pScan >= '1' && *pScan <= '6')
						{
								int id = *pScan - '0';
								int idx = id - 1; 

								float phase_val = LED_get_phaseOffset(idx);
								
								USART_printf("[STS] LED%d Phase Offset: %.2f (%.0f%%)\r\n", 
														 id, phase_val, phase_val * 100.0f);
						}
             pScan++;
        }
    }

    // ============================================================
    // 4. 【兜底处理】: 未知指令
    // ============================================================
    if (isHandled == 0)
    {
        // 过滤空包，但不要丢弃短的未知指令（例如 "A"）
        if (strlen(cmd) > 0)
        {
            USART_printf("[Unknown] I don't understand: %s\r\n", cmd);
        }
    }
}

/* --- HAL Callback Override --- */

// RX 回调保持不变
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
			 if (Size > 0)
			 {
			 	 rt_ringbuffer_put(&s_RxRingCtrl, s_RxDMABuffer, Size);
			 }
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_RxDMABuffer, USART_RX_LINE_MAX);
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
     }
}

/**
 * @brief  TX 发送完成回调
 * @note   当 DMA 把 s_TxDmaBuffer 里的数据发完后，硬件会自动调这个
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // 1. 刚才那次发送结束了，标记为空闲
        b_TxDMABusy = 0;

        // 2. 自我检查：RingBuffer 里还有剩饭吗？
        // 如果有，USART_trigger_TX 会再次置忙并启动下一次 DMA
        USART_trigger_TX();
    }
}
