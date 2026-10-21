#ifndef WAVANL_SERVICE_H
#define WAVANL_SERVICE_H

#include "stdbool.h"
#include "stdint.h"

/* Runs the analyzer on the ADC buffer and caches the result. Call from a task, not an ISR. */
bool wavAnl_update(void);
const char* wavAnl_menu_info(float* period, float* vpp, uint8_t* index);
void wavAnl_print(void);

#endif
