#include <memory_pool.h>
#include "stddef.h"

/* 分类表：块大小从小到大排列，块大小必须是 4 的倍数 */
#define MP_CLASS_NUM    4
#define MP_BLOCK_TOTAL  46      /* = 32 + 8 + 4 + 2，改分类表时同步修改 */
#define MP_NONE         0xFFFFu

static const uint32_t s_class_size[MP_CLASS_NUM]  = {32, 128, 512, 2048};
static const uint16_t s_class_count[MP_CLASS_NUM] = {32,   8,   4,    2};

typedef struct {
  uint8_t  *base;       /* 本类在区域中的起始地址 */
  uint32_t  size;       /* 块大小 */
  uint16_t  count;      /* 块数量 */
  uint16_t  first;      /* 本类第一个块在全局表中的下标 */
  uint16_t  head;       /* 空闲链表头（全局下标） */
  uint16_t  free_cnt;
} MP_class_t;

static uint32_t   s_region[POOL_SIZE / 4];      /* 4 字节对齐的池子区域 */
static MP_class_t s_class[MP_CLASS_NUM];
static uint16_t   s_next[MP_BLOCK_TOTAL];       /* 空闲链表：下一个空闲块的全局下标 */
static uint8_t    s_used[MP_BLOCK_TOTAL];       /* 1 = 已分配 */
static uint8_t    s_ready;

void MP_init(void)
{
  uint8_t  c;
  uint16_t i;
  uint16_t g = 0;
  uint32_t off = 0;
  uint32_t total = 0;
  uint16_t cnt_total = 0;
  MP_class_t *k;

  s_ready = 0;

  /* 分类表超过池子大小或块总数不符，则池子不启用，MP_alloc 一律返回 NULL */
  for (c = 0; c < MP_CLASS_NUM; c++) {
    total += s_class_size[c] * s_class_count[c];
    cnt_total += s_class_count[c];
  }
  if (total > POOL_SIZE || cnt_total != MP_BLOCK_TOTAL) {
    return;
  }

  for (c = 0; c < MP_CLASS_NUM; c++) {
    k = &s_class[c];
    k->base     = (uint8_t *)s_region + off;
    k->size     = s_class_size[c];
    k->count    = s_class_count[c];
    k->first    = g;
    k->free_cnt = k->count;
    k->head     = (k->count > 0) ? g : MP_NONE;

    for (i = 0; i < k->count; i++) {
      s_next[g] = (i + 1 < k->count) ? (uint16_t)(g + 1) : MP_NONE;
      s_used[g] = 0;
      g++;
    }
    off += k->size * k->count;
  }

  s_ready = 1;
}

void *MP_alloc(uint32_t size)
{
  uint8_t c;
  uint16_t idx;
  MP_class_t *k;

  if (!s_ready || size == 0) {
    return NULL;
  }

  /* 类别按块大小升序，第一个能装下 size 的即为最小类别 */
  for (c = 0; c < MP_CLASS_NUM; c++) {
    if (s_class_size[c] >= size) {
      break;
    }
  }
  if (c == MP_CLASS_NUM) {
    return NULL;
  }

  k = &s_class[c];
  if (k->head == MP_NONE) {
    return NULL;        /* 该类已用完，不向其他类借 */
  }

  idx = k->head;
  k->head = s_next[idx];
  s_next[idx] = MP_NONE;
  s_used[idx] = 1;
  k->free_cnt--;

  return k->base + (uint32_t)(idx - k->first) * k->size;
}

void MP_free(void *p)
{
  uint8_t *ptr = (uint8_t *)p;
  uint8_t c;
  uint32_t off;
  uint16_t idx;
  MP_class_t *k = NULL;

  if (!s_ready || ptr == NULL) {
    return;
  }

  for (c = 0; c < MP_CLASS_NUM; c++) {
    k = &s_class[c];
    if (ptr >= k->base && ptr < k->base + k->size * k->count) {
      break;
    }
  }
  if (c == MP_CLASS_NUM) {
    return;             /* 不在池内 */
  }

  off = (uint32_t)(ptr - k->base);
  if (off % k->size != 0) {
    return;             /* 不是块起始地址 */
  }

  idx = (uint16_t)(k->first + off / k->size);
  if (!s_used[idx]) {
    return;             /* 重复释放 */
  }

  s_used[idx] = 0;
  s_next[idx] = k->head;
  k->head = idx;
  k->free_cnt++;
}

uint16_t MP_get_free_count(uint32_t size)
{
  uint8_t c;

  if (!s_ready) {
    return 0;
  }

  for (c = 0; c < MP_CLASS_NUM; c++) {
    if (s_class_size[c] >= size) {
      return s_class[c].free_cnt;
    }
  }
  return 0;
}
