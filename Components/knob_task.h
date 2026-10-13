#ifndef KNOB_TASK_H
#define KNOB_TASK_H

#include "stdint.h"

typedef void (*knob_change_cb_t)(uint32_t value);

void knob_proc(void);
uint32_t knob_get_value();
void knob_register_change_cb(knob_change_cb_t fn_cb);

#endif