#include "stddef.h"
#include <menu.h>

// 状态机全局变量：当前指向的菜单节点
static menu_t *s_pCurrentItem = NULL;

// 初始化菜单，指定根节点
void MENU_init(menu_t *root)
{
    if (root != NULL)
    {
        s_pCurrentItem = root;
    }
}

// 核心导航状态机：响应上下左右/确认返回
void MENU_navigate(e_menuKey_t e_key)
{
    if (s_pCurrentItem == NULL)
    {
        return;
    }

    switch (e_key)
    {
        case MENU_KEY_UP:
            if (s_pCurrentItem->p_prev != NULL)
            {
                s_pCurrentItem = s_pCurrentItem->p_prev;
            }
            break;

        case MENU_KEY_DOWN:
            if (s_pCurrentItem->p_next != NULL)
            {
                s_pCurrentItem = s_pCurrentItem->p_next;
            }
            break;

        case MENU_KEY_ENTER:
            // 如果有子菜单，则深入子菜单
            if (s_pCurrentItem->p_child != NULL)
            {
                s_pCurrentItem = s_pCurrentItem->p_child;
            }
            // 如果没有子菜单但有执行函数，则调用具体业务
            else if (s_pCurrentItem->action != NULL)
            {
                s_pCurrentItem->action();
            }
            break;

        case MENU_KEY_BACK:
            // 如果存在父节点，退回上一级菜单
            if (s_pCurrentItem->p_parent != NULL)
            {
                s_pCurrentItem = s_pCurrentItem->p_parent;
            }
            break;

        default:
            break;
    }
}

// 获取当前选中的节点（供 UI 显示读取名称等信息）
const menu_t *menu_get_current_item(void)
{
    return s_pCurrentItem;
}