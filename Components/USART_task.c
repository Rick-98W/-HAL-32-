#include <USART_task.h>
#include "usart.h"
#include <LED_task.h>
#include <DAC_service.h>
#include <ADC_DAC_sync.h>
#include <ADC_service.h>
#include <wavAnl_service.h>
#include <menu_ADC_suppl.h>
#include <serial_export_task.h>
#include "ringbuffer.h" // ������������ RingBuffer
#include "stdio.h"
#include "stdarg.h"
#include "string.h"

/* --- Configuration --- */
#define USART_RX_RING_SIZE   2048
#define USART_RX_LINE_MAX    256     // ����ָ����󳤶�

#define USART_TX_RING_SIZE   2048    // ��������TX RingBuffer ��С (�㹻���Ի���ͻ����־)
#define USART_TX_DMA_SIZE    512     // ������������ DMA ���˵���󳤶�

/* --- Private Variables --- */

// 1. �������
static struct rt_ringbuffer s_RxRingCtrl;
static uint8_t s_RxRingPool[USART_RX_RING_SIZE];
static uint8_t s_RxDMABuffer[USART_RX_LINE_MAX];       // DMA д����

// ����APP �õġ����ԡ��ݴ��� (��� RingBuffer �ؾ�����)
static char s_RxAppBuffer[USART_RX_LINE_MAX];
static uint16_t s_RxLineIdx = 0;
static uint8_t s_RxLineOverflow = 0;                   /* set when a line exceeds the max; the whole line is dropped */                       // ָ��ƴ���α�

// 2. �������
// RingBuffer ���ƿ����ڴ��
static struct rt_ringbuffer s_TxRingCtrl;
static uint8_t s_TxRingPool[USART_TX_RING_SIZE];

// DMA ר�õġ����ԡ��ݴ��� (��� RingBuffer �ؾ�����)
static uint8_t s_TxDMABuffer[USART_TX_DMA_SIZE];

// ����״̬��־ (1: DMA����æ, 0: DMA����)
static volatile uint8_t b_TxDMABusy = 0;

// bytes dropped because the TX ring buffer was full (reported from USART_proc)
static volatile uint32_t s_TxDropped = 0;

/* --- Private Function Prototypes --- */
static void USART_parse_command(char *cmd);
static void USART_trigger_TX(void);  //�����ѷ���������
static uint8_t USART_stream_load(void); //ɨ��Rx_RingBuffer�Գ�ȡcommand��parse����

/* --- Public Function Implementation --- */

void USART_init(void)
{
    // --- RX Init ---
		rt_ringbuffer_init(&s_RxRingCtrl, s_RxRingPool, USART_RX_RING_SIZE);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_RxDMABuffer, USART_RX_LINE_MAX);
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

    // --- TX Init ---
    // ��ʼ�� RingBuffer
    rt_ringbuffer_init(&s_TxRingCtrl, s_TxRingPool, USART_TX_RING_SIZE);
    
    // ��ʼ״̬ DMA �����ŵ�
    b_TxDMABusy = 0;
}

void USART_proc(void)
{
		if (s_TxDropped) {
				uint32_t dropped = s_TxDropped;
				s_TxDropped = 0;
				USART_printf("[WRN] TX buffer full, %lu bytes dropped\r\n", (unsigned long)dropped);
			}
			if (USART_stream_load()){
			USART_parse_command(s_RxAppBuffer);
		}
}

// ������ printf��ֻ�������������꺰һ�� DMA
void USART_printf(const char* fmt, ...)
{
    // 1. ��ʽ�����ݵ��ֲ�ջ�ռ� (�ٶȼ���)
    char temp_buf[256]; 
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(temp_buf, sizeof(temp_buf), fmt, args);
    va_end(args);

    if (len <= 0) return;
		if (len > (int)sizeof(temp_buf) - 1) len = sizeof(temp_buf) - 1;

    // 2. ѹ�� RingBuffer (Critical Section)
    // ��Ϊ TxCpltCallback �ж���Ҳ����� RingBuffer (get)���������������жϱ��� put
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    // ʹ�ñ�׼ put�����˾Ͷ��������ݣ���֤��ȫ��
    rt_size_t put = rt_ringbuffer_put(&s_TxRingCtrl, (uint8_t *)temp_buf, len);
    if (put < (rt_size_t)len) {
        s_TxDropped += len - put;
    }

    // 3. �����߼������ DMA �Ƿ���͵��
    if (b_TxDMABusy == 0)
    {
        // ��� DMA ���ţ��Ͻ�����һ�ţ�������ʼ�ɻ�
        USART_trigger_TX();
    }
    
    __set_PRIMASK(primask);
}

/* --- HAL Callback Override --- */

// RX �ص����ֲ���
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
 * @brief  TX ������ɻص�
 * @note   �� DMA �� s_TxDmaBuffer ������ݷ����Ӳ�����Զ������
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // 1. �ղ��Ǵη��ͽ����ˣ����Ϊ����
        b_TxDMABusy = 0;

        // 2. ���Ҽ�飺RingBuffer �ﻹ��ʣ����
        // ����У�USART_trigger_TX ���ٴ���æ��������һ�� DMA
        USART_trigger_TX();
    }
}

/* --- Private Function Implementation --- */

/**
 * @brief  �����ѷ��������� (DMA ������)
 * @note   ����� RingBuffer �������ݵ����� Buffer�������� DMA
 * ��������Ȼᱻ printf ���ã�Ҳ�ᱻ�жϻص�����
 */
static void USART_trigger_TX(void)
{
    // 1. �ٴ�ȷ��æ��״̬ (��Ȼ����ǰͨ���жϹ�����˫�ر���û��)
    if (b_TxDMABusy) return;

    // 2. �� RingBuffer ȡ��
    // rt_ringbuffer_get ���Զ������ؾ��������� read_index
    // ����ֵ��ʵ��ȡ�����ֽ���
    rt_size_t len = rt_ringbuffer_get(&s_TxRingCtrl, s_TxDMABuffer, USART_TX_DMA_SIZE);

    if (len > 0)
    {
        // 3. �л������״̬Ϊæ
        b_TxDMABusy = 1;

        // 4. ���� DMA ���� (������)
        // ע�⣺����ʹ�õ������ǰ��˳����� s_TxDmaBuffer (���������ڴ�)
        if (HAL_UART_Transmit_DMA(&huart1, s_TxDMABuffer, len) != HAL_OK)
        {
            // �������ʧ�� (������)�����æ��־����ֹ����
            b_TxDMABusy = 0;
        }
    }
    else
    {
        // û���ˣ�DMA ������Ϣ��
        b_TxDMABusy = 0;
    }
}

static uint8_t USART_stream_load(void)
{
    uint8_t ch;
    
    // ÿ�δ���һ���ַ�
    while(rt_ringbuffer_getchar(&s_RxRingCtrl, &ch)){
        
        // 1. �������жϣ�����Э��ֺźʹ��ڹ��߳��õ� CR/LF
        if (ch == ';' || ch == '\n'){
            if (s_RxLineOverflow) {
                /* over-long line: discard it entirely, do not hand a truncated command to the parser */
                s_RxLineOverflow = 0;
                s_RxLineIdx = 0;
                continue;
            }
            s_RxAppBuffer[s_RxLineIdx] = '\0'; // ���
            s_RxLineIdx = 0; // ����
            return 1;        // �ɹ���
        }

        // CRLF �� CR ֻ��Ϊ�н�����һ���֣����Ž������ַ���
        if (ch == '\r') {
            continue;
        }
        
        // 2. �������루�����������
        // ��һ��λ�ø� \0�������� MAX - 1
        if (s_RxLineIdx < USART_RX_LINE_MAX - 1) {
            s_RxAppBuffer[s_RxLineIdx++] = ch;
        }
        else {
            // ��������������ѡ��������������߶���������
            // ����򵥲��ԣ����ã�������һ��ָ������
            s_RxLineOverflow = 1;
        }
    }
    return 0; 
}

static void LED_dispatch_command(char* p_subCmd);
static void ADC_dispatch_command(char* p_subCmd);
static void DAC_dispatch_command(char* p_subCmd);
static void SYS_dispatch_command(char* p_subCmd);

// ============================================================
// Main Dispatcher: USART_parse_command
// ============================================================
static void USART_parse_command(char* cmd) {
    if (cmd == NULL || *cmd == '\0' || *cmd == '\r' || *cmd == '\n') {
        return;
    }

    while (*cmd == ' ' || *cmd == '\t') {
        cmd++;
    }

    if (*cmd == '\0' || *cmd == '\r' || *cmd == '\n') {
        return;
    }

    USART_printf("Command Received: %s\r\n", cmd);

    if (strncmp(cmd, "LED", 3) == 0) {
        LED_dispatch_command(cmd + 3);
    }
    else if (strncmp(cmd, "ADC", 3) == 0) {
        ADC_dispatch_command(cmd + 3);
    }
    else if (strncmp(cmd, "DAC", 3) == 0) {
        DAC_dispatch_command(cmd + 3);
    }
    else if (strncmp(cmd, "SYS", 3) == 0) {
        SYS_dispatch_command(cmd + 3);
    }
    else {
        USART_printf("[UNK] Invalid category prefix: %s\r\n", cmd);
    }
}

// ============================================================
// Module Handler Definitions
// ============================================================
static void LED_dispatch_command(char* p_subCmd) {
    if (strstr(p_subCmd, "toggle") != NULL) {
        char *p_scan = strstr(p_subCmd, "toggle") + 6;
        uint8_t toggle_cnt = 0;

        while (*p_scan != '\0' && *p_scan != '\r' && *p_scan != '\n') {
            if (*p_scan >= '1' && *p_scan <= '6') {
                int id = *p_scan - '0';
                LED_toggle_phase(id - 1);
                USART_printf("[CMD] LED toggled: LED%d\r\n", id);
                toggle_cnt++;
            }
            p_scan++;
        }
        if (toggle_cnt == 0) {
            USART_printf("[WRN] LED toggle found, but no ID (1-6)!\r\n");
        }
    }
    else if (strstr(p_subCmd, "phase") != NULL) {
        char *p_scan = p_subCmd;
        while (*p_scan != '\0' && *p_scan != '\r' && *p_scan != '\n') {
            if (*p_scan >= '1' && *p_scan <= '6') {
                int id = *p_scan - '0';
                float phase_val = LED_get_phaseOffset(id - 1);
                USART_printf("[STS] LED%d Phase Offset: %.2f (%.0f%%)\r\n", 
                             id, phase_val, phase_val * 100.0f);
            }
            p_scan++;
        }
    }
    else if (strstr(p_subCmd, "off") != NULL) {
        LED_set_blackout(1);
        USART_printf("[CMD] LED: OFFLINE (Blackout)\r\n");
    }
    else if (strstr(p_subCmd, "on") != NULL) {
        LED_set_blackout(0);
        USART_printf("[CMD] LED: ONLINE\r\n");
    }
    else {
        USART_printf("[WRN] LED command recognized, but missing valid action (toggle/phase/on/off)!\r\n");
    }
}

// Reuse the cached result while settings are unchanged; otherwise analyze again.
static bool ADC_ensure_wave_result(void) {
    if (wavAnl_is_valid()) return true;
    if (!wavAnl_update()) return false;
    ADC_menu_refresh_wave();
    return true;
}

static void ADC_dispatch_command(char* p_subCmd) {
    if (strstr(p_subCmd, "analyze") != NULL) {
        if (wavAnl_update()) {
            USART_printf("[CMD] ADC wave analyzed\r\n");
            ADC_menu_refresh_wave();
        }
    }
    else if (strstr(p_subCmd, "set ratio double") != NULL) {
        ADC_set_ratio(1);
				ADC_menu_refresh_ratio();
        USART_printf("[CMD] ADC ratio: double\r\n");
    }
    else if (strstr(p_subCmd, "set ratio single") != NULL) {
        ADC_set_ratio(0);
				ADC_menu_refresh_ratio();
        USART_printf("[CMD] ADC ratio: single\r\n");
    }
    else if (strstr(p_subCmd, "get ratio") != NULL) {
        USART_printf("[STS] ADC ratio: %s\r\n", ADC_get_ratio() ? "double" : "single");
    }
    else if (strstr(p_subCmd, "get freq") != NULL) {
        if (ADC_ensure_wave_result()) {
            USART_printf("[STS] ADC freq: %.1f Hz\r\n", wavAnl_get_freq());
        }
    }
    else if (strstr(p_subCmd, "get period") != NULL) {
        if (ADC_ensure_wave_result()) {
            USART_printf("[STS] ADC period: %.3f ms\r\n", wavAnl_get_period() * 1000.0f);
        }
    }
    else if (strstr(p_subCmd, "get vpp") != NULL) {
        if (ADC_ensure_wave_result()) {
            USART_printf("[STS] ADC vpp: %.0f mV\r\n", wavAnl_get_vpp() * 1000.0f);
        }
    }
    else if (strstr(p_subCmd, "get type") != NULL) {
        if (ADC_ensure_wave_result()) {
            USART_printf("[STS] ADC wave type: %s\r\n", wavAnl_get_type_name());
        }
    }
    else if (strstr(p_subCmd, "wave export") != NULL) {
        serial_export_ADC_wave();
    }
    else {
        USART_printf("[WRN] ADC command recognized, but missing valid action (analyze/set ratio/get ratio/get freq/get period/get vpp/get type)!\r\n");
    }
}

static int16_t DAC_clamp_int16(long v) {
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

static const char* waveType[3] = {
	"sinusoidal","triangular","sawtooth"
};

static void DAC_cmd_strcmp(char* p_subCmd) {
	if (strstr(p_subCmd, "sin") != NULL) {
		ADC_DAC_sync_set_wave(SINUSOIDAL);
	}
	else if (strstr(p_subCmd, "tri") != NULL) {
		ADC_DAC_sync_set_wave(TRIANGULAR);
	}
	else if (strstr(p_subCmd, "saw") != NULL) {
		ADC_DAC_sync_set_wave(SAWTOOTH);
	}
	USART_printf("[CMD] DAC wave type: %s\r\n", waveType[DAC_get_waveType()]);
}

static void DAC_dispatch_command(char* p_subCmd) {
    long val;

    if (sscanf(p_subCmd, " set FSR %ld", &val) == 1) {
        ADC_DAC_sync_set_FSR(DAC_clamp_int16(val));
        USART_printf("[CMD] DAC FSR: %d\r\n", DAC_get_FSR());
				DAC_menu_refresh_FSR();
    }
    else if (sscanf(p_subCmd, " set period %ld", &val) == 1) {
        ADC_DAC_sync_set_period(DAC_clamp_int16(val));
        USART_printf("[CMD] DAC period: %d\r\n", DAC_get_period());
				DAC_menu_refresh_period();
    }
    else if (strstr(p_subCmd, "set wave type") != NULL) {
        DAC_cmd_strcmp(p_subCmd);
				DAC_menu_refresh_waveType();
    }
    else if (strstr(p_subCmd, "get FSR") != NULL) {
        USART_printf("[STS] DAC FSR: %d\r\n", DAC_get_FSR());
    }
    else if (strstr(p_subCmd, "get period") != NULL) {
        USART_printf("[STS] DAC period: %d\r\n", DAC_get_period());
    }
    else if (strstr(p_subCmd, "get wave type") != NULL) {
        USART_printf("[STS] DAC wave type: %s\r\n", waveType[DAC_get_waveType()]);
    }
    else {
        USART_printf("[WRN] DAC command recognized, but missing valid action (set FSR/set period/get FSR/get period)!\r\n");
    }
}

static void SYS_dispatch_command(char* p_subCmd) {
    // TODO: Implement specific SYS parsing logic here
    USART_printf("[SYS] SYS Sub-command received: %s\r\n", p_subCmd);
}
