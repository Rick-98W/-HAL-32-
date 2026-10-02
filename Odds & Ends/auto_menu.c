#include "auto_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 实现 strdup 函数，用于复制字符串
char *strdup(const char *s)
{
    if (s == NULL)
        return NULL;

    size_t len = strlen(s) + 1; // 加 1 是为了包括末尾的 '\0'
    char *copy = (char *)malloc(len);

    if (copy != NULL)
    {
        memcpy(copy, s, len); // 复制字符串内容
    }

    return copy;
}

// 定义全局变量
static MenuItem *current_menu = NULL;                // 当前菜单指针
static MenuItem *parent_menu_stack[MENU_STACK_SIZE]; // 父菜单堆栈
static int parent_menu_stack_index = -1;             // 堆栈索引
static int current_level = 0;                        // 当前菜单层级

// 创建一个菜单项
MenuItem *create_menu_item(const char *name, MenuItemType type, void (*action)(void), MenuItem *submenu)
{
    MenuItem *item = (MenuItem *)malloc(sizeof(MenuItem));
    if (!item)
    {
        return NULL; // 内存分配失败时返回 NULL
    }
    item->name = strdup(name); // 复制菜单项名称
    item->type = type;
    if (type == MENU_ITEM_ACTION)
    {
        item->menu_item.action = action; // 为动作类型设置回调函数
    }
    else if (type == MENU_ITEM_SUBMENU)
    {
        item->menu_item.submenu = submenu; // 为子菜单类型设置子菜单指针
    }
    else if (type == MENU_ITEM_BOOLEAN)
    {
        item->menu_item.boolean_item.value = NULL;         // 初始设置为空，需在外部设置
        item->menu_item.boolean_item.toggle_action = NULL; // 初始设置为空，需在外部设置
    }
    else if (type == MENU_ITEM_NUMERIC)
    {
        item->menu_item.numeric_item.value = NULL;         // 初始设置为空，需在外部设置
        item->menu_item.numeric_item.min = 0;              // 初始最小值为0
        item->menu_item.numeric_item.max = 100;            // 初始最大值为100
        item->menu_item.numeric_item.adjust_action = NULL; // 初始设置为空，需在外部设置
    }
    item->next = NULL;
    item->prev = NULL;
    return item;
}

// 将菜单项添加到链表中
void add_menu_item(MenuItem **head, MenuItem *item)
{
    if (*head == NULL)
    {
        *head = item; // 如果链表为空，将新项设为头节点
    }
    else
    {
        MenuItem *temp = *head;
        while (temp->next != NULL)
        {
            temp = temp->next; // 遍历到链表末尾
        }
        temp->next = item; // 将新项添加到链表末尾
        item->prev = temp; // 设置前一项的指针
    }
}

// 删除菜单项
void remove_menu_item(MenuItem **head, const char *name)
{
    if (!head || !*head || !name)
        return;

    MenuItem *temp = *head;
    while (temp != NULL)
    {
        if (strcmp(temp->name, name) == 0)
        {
            if (temp->prev)
                temp->prev->next = temp->next;
            if (temp->next)
                temp->next->prev = temp->prev;

            if (temp == *head)
                *head = temp->next;

            free(temp->name);
            free(temp);
            return;
        }
        temp = temp->next;
    }
}

// 更新菜单项
void update_menu_item(MenuItem *item, const char *new_name, void (*new_action)(void))
{
    if (!item || !new_name)
        return;

    free(item->name);
    item->name = strdup(new_name);
    if (item->type == MENU_ITEM_ACTION)
    {
        item->menu_item.action = new_action;
    }
}

// 释放菜单结构的内存
void free_menu(MenuItem *menu)
{
    MenuItem *temp;
    while (menu)
    {
        temp = menu;
        menu = menu->next;
        free(temp->name); // 释放菜单项名称的内存
        free(temp);       // 释放菜单项的内存
    }
}

// 初始化菜单
void auto_menu_init(MenuItem *root)
{
    current_menu = root;          // 设置当前菜单为根菜单
    parent_menu_stack_index = -1; // 重置父菜单堆栈索引
    current_level = 0;            // 重置菜单层级
    auto_menu_display();          // 初始化后立即显示菜单
}

uint8_t para_set_flag = 0; // 数值设置模式标志

// 处理按键操作
void auto_menu_handle_key(uint8_t key)
{
    // 如果当前菜单项是数值类型，使用专用的设置逻辑
    if (current_menu && current_menu->type == MENU_ITEM_NUMERIC && para_set_flag == 1)
    {
        switch (key)
        {
        case 1: // 增加数值
            if (current_menu->menu_item.numeric_item.adjust_action)
            {
                current_menu->menu_item.numeric_item.adjust_action(1); // 调增1
            }
            break;
        case 2: // 减少数值
            if (current_menu->menu_item.numeric_item.adjust_action)
            {
                current_menu->menu_item.numeric_item.adjust_action(-1); // 调减1
            }
            break;
        case 4: // 退出数值设置
            para_set_flag = 0; // 退出数值设置模式
            break;
        }
    }
    else
    {
        // 处理非设置模式下的菜单导航逻辑
        switch (key)
        {
        case 1: // 向上移动
            if (current_menu && current_menu->prev != NULL)
            {
                current_menu = current_menu->prev;
            }
            else
            {
                while (current_menu && current_menu->next != NULL)
                {
                    current_menu = current_menu->next;
                }
            }
            break;
        case 2: // 向下移动
            if (current_menu && current_menu->next != NULL)
            {
                current_menu = current_menu->next;
            }
            else
            {
                while (current_menu && current_menu->prev != NULL)
                {
                    current_menu = current_menu->prev;
                }
            }
            break;
        case 3: // 选择或操作当前项
            if (current_menu)
            {
                switch (current_menu->type)
                {
                case MENU_ITEM_ACTION:
                    if (current_menu->menu_item.action)
                    {
                        current_menu->menu_item.action(); // 执行动作
                    }
                    break;

                case MENU_ITEM_SUBMENU:
                    if (parent_menu_stack_index < MENU_STACK_SIZE - 1)
                    {
                        parent_menu_stack[++parent_menu_stack_index] = current_menu; // 压入堆栈
                        current_menu = current_menu->menu_item.submenu;              // 进入子菜单
                        current_level++;                                             // 增加层级
                    }
                    break;

                case MENU_ITEM_BOOLEAN:
                    if (current_menu->menu_item.boolean_item.toggle_action)
                    {
                        current_menu->menu_item.boolean_item.toggle_action(); // 切换布尔值
                    }
                    break;

                case MENU_ITEM_NUMERIC:
                    para_set_flag = 1; // 进入数值设置模式
                    break;
                }
            }
            break;
        case 4: // 返回上一级菜单
            if (parent_menu_stack_index >= 0)
            {
                current_menu = parent_menu_stack[parent_menu_stack_index--];
                current_level--;
            }
            break;
        }
    }

    auto_menu_display(); // 刷新菜单显示
}

void auto_menu_display(void)
{
    if (current_menu == NULL)
    {
        menu_init(); // 如果当前菜单为空，重新初始化菜单
        return;
    }

    LCD_Clear(Black); // 清除 LCD 显示

    LCD_SetTextColor(White); // 设置默认的字体颜色为白色
    LCD_SetBackColor(Black); // 设置默认的背景颜色为黑色

    MenuItem *temp = current_menu;
    uint8_t line = 0;

    // 显示当前层级
    char level_str[20];
    snprintf(level_str, sizeof(level_str), para_set_flag?"Level: %d (Setting)":"Level: %d", current_level);
    LCD_DisplayStringLine(line++ * 24, level_str);

    // 查找当前选中项的第一个可显示的菜单项
    while (temp->prev != NULL && line < 4)
    {
        temp = temp->prev;
        line++;
    }

    // 重置行计数器
    line = 1;

    // 遍历从当前选中项开始的菜单项并显示
    while (temp != NULL && line < MAX_LINES)
    {
        if (temp->name == NULL || temp->name[0] == '\0')
        {
            break;
        }

        // 如果当前菜单项是选中项，使用反转颜色
        if (temp == current_menu)
        {
            LCD_SetTextColor(Black);
            LCD_SetBackColor(White);
        }
        else
        {
            LCD_SetTextColor(White);
            LCD_SetBackColor(Black);
        }

        // 构建菜单项显示文本
        char display_text[50];
        snprintf(display_text, sizeof(display_text), "%s", temp->name);

        // 根据菜单项类型显示附加信息
        if (temp->type == MENU_ITEM_BOOLEAN && temp->menu_item.boolean_item.value != NULL)
        {
            strcat(display_text, (*(temp->menu_item.boolean_item.value) ? " ON" : " OFF"));
        }
        else if (temp->type == MENU_ITEM_NUMERIC && temp->menu_item.numeric_item.value != NULL)
        {
            char value_str[10];
            snprintf(value_str, sizeof(value_str), " %d", *(temp->menu_item.numeric_item.value));
            strcat(display_text, value_str);
        }

        LCD_DisplayStringLine(line * 24, display_text); // 显示菜单项

        temp = temp->next; // 遍历兄弟项
        line++;
    }
}

#include <stdio.h>

uint8_t bool_val = 0; // 全局布尔值变量
int para_val = 50;    // 全局数值变量

// 动作回调函数示例
void action_1(void)
{
    printf("Action 1 triggered!\n");
}

void action_2(void)
{
    printf("Action 2 triggered!\n");
}

// 布尔开关的回调函数
void toggle_bool(void)
{
    bool_val = !bool_val; // 切换布尔值
    printf("Boolean toggled: %d\n", bool_val);
}

// 数值调整的回调函数
void adjust_value(int adjustment)
{
    para_val += adjustment; // 调整数值
    if (para_val < 0)
        para_val = 0;
    if (para_val > 100)
        para_val = 100;
    printf("Value adjusted: %d\n", para_val);
}

void menu_init(void)
{
    // 初始化菜单结构，配置菜单项

    MenuItem *root_menu = create_menu_item("Main Menu", MENU_ITEM_SUBMENU, NULL, NULL);
    MenuItem *submenu1 = create_menu_item("SubMenu 1", MENU_ITEM_SUBMENU, NULL, NULL);
    MenuItem *action_item1 = create_menu_item("Action 1", MENU_ITEM_ACTION, action_1, NULL);
    MenuItem *boolean_item = create_menu_item("Toggle Bool", MENU_ITEM_BOOLEAN, NULL, NULL);
    boolean_item->menu_item.boolean_item.value = &bool_val;
    boolean_item->menu_item.boolean_item.toggle_action = toggle_bool;

    MenuItem *numeric_item = create_menu_item("Adjust Value", MENU_ITEM_NUMERIC, NULL, NULL);
    numeric_item->menu_item.numeric_item.value = &para_val;
    numeric_item->menu_item.numeric_item.min = 0;
    numeric_item->menu_item.numeric_item.max = 100;
    numeric_item->menu_item.numeric_item.adjust_action = adjust_value;

    // 构建菜单层次结构
    add_menu_item(&root_menu->menu_item.submenu, action_item1);
    add_menu_item(&root_menu->menu_item.submenu, submenu1);
    add_menu_item(&submenu1->menu_item.submenu, boolean_item);
    add_menu_item(&submenu1->menu_item.submenu, numeric_item);

    // 初始化菜单系统
    auto_menu_init(root_menu);
}
