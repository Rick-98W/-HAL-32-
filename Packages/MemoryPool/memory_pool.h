#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include "stdint.h"

#define POOL_SIZE   8192    /* 池子总字节数，MP_init 时按 memory_pool.c 的分类表划分 */

void      MP_init(void);
void     *MP_alloc(uint32_t size);      /* 选能容纳 size 的最小类别，该类满则返回 NULL */
void      MP_free(void *p);             /* 非池内指针、非块起始、重复释放均忽略 */
uint16_t  MP_get_free_count(uint32_t size);

#endif
