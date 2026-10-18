#include <scheduler.h>

#include <LED_task.h>
#include <button_task.h>
#include <USART_task.h>
#include <serial_export_task.h>
#include <knob_task.h> 

// ȫ�ֱ��������ڴ洢��������
uint8_t task_num;

typedef struct {
    void (*fn_task_func)(void);
    uint32_t rate_ms;
    uint32_t last_run;
} task_t;

// ��̬�������飬ÿ�����������������ִ�����ڣ����룩���ϴ�����ʱ�䣨���룩
static task_t scheduler_task[] =
{
		{LED_proc, 1, 0},
		{button_proc, 5, 0},
		{USART_proc, 1, 0},
			{serial_export_proc, 1, 0},
		{knob_proc, 20, 0},
};

/**
 * @brief ��������ʼ������
 * �������������Ԫ�ظ�������������洢�� task_num ��
 */
void scheduler_init(void){
    // �������������Ԫ�ظ�������������洢�� task_num ��
    task_num = sizeof(scheduler_task) / sizeof(task_t);
}

/**
 * @brief ���������к���
 * �����������飬����Ƿ���������Ҫִ�С������ǰʱ���Ѿ����������ִ�����ڣ���ִ�и����񲢸����ϴ�����ʱ��
 */
void scheduler_run(void){
    // �������������е���������
    for (uint8_t i = 0; i < task_num; i++)
    {
        // ��ȡ��ǰ��ϵͳʱ�䣨���룩
        uint32_t current_tick = HAL_GetTick();

        // ��鵱ǰʱ���Ƿ�ﵽ�����ִ��ʱ��
        if (current_tick - scheduler_task[i].last_run >= scheduler_task[i].rate_ms)
        {
            // ����������ϴ�����ʱ��Ϊ��ǰʱ��
            scheduler_task[i].last_run = current_tick;

            // ִ��������
            scheduler_task[i].fn_task_func();
        }
    }
}