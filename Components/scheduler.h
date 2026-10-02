#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <LED_task.h>
#include <button_task.h>
#include <USART_task.h>

void scheduler_init(void);
void scheduler_run(void);

#endif