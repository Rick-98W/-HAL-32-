#ifndef SERIAL_EXPORT_TASK_H
#define SERIAL_EXPORT_TASK_H

#include <stdint.h>
#include <stdbool.h>

typedef uint16_t (*serial_export_src_t)(void *ctx, uint32_t index);
typedef void (*serial_export_done_t)(void);

bool serial_export_start(serial_export_src_t src, void *ctx, uint32_t count, serial_export_done_t on_done);
bool serial_export_busy(void);
void serial_export_proc(void);

#endif
