#ifndef MENU_H
#define MENU_H

#include "stdint.h"
#include "stdbool.h"

// 统一按键事件枚举
typedef enum
{
    MENU_KEY_UP,
    MENU_KEY_DOWN,
    MENU_KEY_ENTER,
    MENU_KEY_BACK
} e_menuKey_t;

// 极简多级菜单节点结构体
typedef struct menu_t
{
    const char *p_name;         // 节点显示名称
    struct menu_t *p_next;     // 同级下一个选项
    struct menu_t *p_prev;     // 同级上一个选项
    struct menu_t *p_parent;   // 父节点（按 BACK 返回的目标）
    struct menu_t *p_child;    // 首个子节点（按 ENTER 钻入的目标）
    void (*action)(void);    // 如果是终点节点，触发的回调函数（可为空 NULL）
} menu_t;

// 核心导航 API 接口
void MENU_init(menu_t *root);
void MENU_navigate(e_menuKey_t e_key);
const menu_t *menu_get_current_item(void);

#endif