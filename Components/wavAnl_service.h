#ifndef WAVANL_SERVICE_H
#define WAVANL_SERVICE_H

#include "stdbool.h"
#include "stdint.h"

/* Runs the analyzer on the ADC buffer and caches the result. Call from a task, not an ISR. */
bool wavAnl_update(void);
const char* wavAnl_menu_info(float* period, float* vpp, uint8_t* index);
void wavAnl_print(void);

/* Getters for the cached result. A result is valid only while the DAC/ADC
   settings it was computed under are unchanged; otherwise call wavAnl_update(). */
bool wavAnl_is_valid(void);
float wavAnl_get_freq(void);
float wavAnl_get_period(void);
float wavAnl_get_vpp(void);
const char* wavAnl_get_type_name(void);

#endif
