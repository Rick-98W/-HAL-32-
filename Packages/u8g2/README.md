# u8g2 / u8x8 裁剪组件说明（GD32 模板）

本文只按本目录当前实际存在的头文件和源文件整理，避免把完整版 u8g2 的接口误认为本组件都支持。

结论先说：

- 这是一个源码级的 u8g2/u8x8 显示组件，不是 GD32 BSP；I2C/SPI、GPIO、复位和延时必须由应用提供回调。
- 当前真正实现的高级 setup 只有 **u8g2_Setup_ssd1306_i2c_128x32_univision_f()** 和 **u8g2_Setup_null()**。
- 当前真正实现的硬件显示回调只有 SSD1306 128×32 的 univision、winstar，以及 SH1106 128×32 visionox 三种。
- **u8g2.h、u8x8.h 中的声明数量远多于本目录的实现数量**；“头文件里能搜到”不代表“本目录有实现”。

## 1. 分层关系和适用场景

~~~text
应用：界面、图形、日志
          │
          ▼
u8g2：像素绘图 + 字体 + RAM 缓冲
          │ 复用
          ▼
u8x8：8×8 Tile、显示控制器和通信抽象
          │
          ├── display callback：控制器初始化、写 Tile
          ├── CAD callback：命令/参数/数据如何组织
          ├── byte callback：实际 I2C/SPI 字节传输
          └── GPIO/delay callback：RESET、DC、CS、时序和延时
~~~

- **u8g2** 适合像素级图形、中文字体、图标、曲线和完整界面；内容先画到 RAM，再刷新到显示器。
- **u8x8** 不维护完整像素帧缓冲，直接按 8×8 Tile 或字符写屏，RAM 更小，适合简单状态页、日志和字符菜单。
- 两层共用 u8x8_t 的底层回调；不要把 u8g2 的像素坐标和 u8x8 的 Tile 坐标混用。

## 2. 当前裁剪版的真实支持矩阵

| 功能 | 当前实现 | 代码依据和限制 |
|---|---|---|
| u8g2 高级 setup | u8g2_Setup_ssd1306_i2c_128x32_univision_f() | 位于 u8g2_d_setup.c |
| 空设备 setup | u8g2_Setup_null() | 位于 u8g2_setup.c，可用于无硬件绘图测试 |
| u8g2 显示回调 | 间接使用 u8x8_d_ssd1306_128x32_univision | setup 固定选择 fast I2C CAD |
| u8x8 显示回调 | u8x8_d_ssd1306_128x32_univision、u8x8_d_ssd1306_128x32_winstar、u8x8_d_sh1106_128x32_visionox | 都位于 u8x8_d_ssd1306_128x32.c |
| 测试/空设备回调 | u8x8_d_null_cb；u8x8_d_capture（需定义 `NOT_YET_IMPLEMENTED_U8X8_SCREEN_CAPTURE`） | 分别用于无硬件空设备和 Tile 捕获，不属于实际 OLED 控制器 |
| u8g2 缓冲 | u8g2_m_16_4_f() | 16 Tile × 4 Tile × 8 字节 = 512 字节，且是静态共享缓冲 |
| I2C CAD | u8x8_cad_ssd13xx_fast_i2c() | 命令控制字 0x00、数据控制字 0x40，数据分块不超过 24 字节 |
| 软件总线 | u8x8_byte_sw_i2c()、u8x8_byte_4wire_sw_spi() 等 | 仍需 GPIO/delay 回调配合 |

因此，下面这些常见名称虽然可能出现在 u8g2.h 的声明中，但在本目录没有对应实现，不能直接调用：

~~~text
u8g2_Setup_ssd1306_i2c_128x64_noname_f
u8g2_Setup_sh1106_i2c_128x64_noname_f
u8g2_Setup_ssd1306_128x32_univision_1
u8g2_Setup_ssd1306_128x32_univision_2
~~~

否则通常会在链接阶段得到 undefined reference / unresolved external。要支持其他分辨率或控制器，需要把相应的显示驱动、setup 和内存实现补进工程，不能只改函数名。

## 3. 文件用途速查

| 文件组 | 用途 |
|---|---|
| u8g2.h、u8g2_setup.c | u8g2 对象、旋转、裁剪、状态和通用 setup |
| u8g2_buffer.c、u8g2_d_memory.c | 全屏/分页缓冲、刷新、当前版本的 512 字节缓冲 |
| u8g2_font.c、u8g2_fonts.c | 字体解码、UTF-8、字体数据 |
| u8g2_hvline.c、u8g2_ll_hvline.c、u8g2_line.c | 像素、水平/垂直线、底层位图布局 |
| u8g2_box.c、u8g2_circle.c、u8g2_arc.c、u8g2_polygon.c | 矩形、圆/椭圆、弧线、多边形 |
| u8g2_bitmap.c | Bitmap、XBM/XBMP |
| u8g2_button.c | 按钮边框、反色按钮、单选框 |
| u8g2_intersection.c、u8g2_kerning.c | 相交裁剪和字距接口 |
| u8g2_selection_list.c、u8g2_message.c、u8g2_input_value.c | 阻塞式选择列表、消息框、数值输入 |
| u8g2_cleardisplay.c | 兼容各种控制器的整屏清除 |
| u8x8.h、u8x8_setup.c、u8x8_display.c | Tile 层对象、初始化和直接写屏 |
| u8x8_cad.c、u8x8_byte.c、u8x8_gpio.c | 命令/参数/数据、字节传输、GPIO/延时适配 |
| u8x8_d_ssd1306_128x32.c | 本裁剪版实际保留的三个显示回调 |
| u8x8_8x8.c、u8x8_string.c、u8x8_fonts.c | u8x8 字符绘制、字符串和字体数据 |
| u8x8_u8toa.c、u8x8_u16toa.c | u8x8 数值转字符串；u8log 的十进制输出会依赖它们 |
| u8x8_selection_list.c、u8x8_message.c、u8x8_input_value.c | u8x8 阻塞式选择列表、消息框和数值输入 |
| mui.c/.h、mui_u8g2.c/.h | MUI 菜单/表单核心及 u8g2 绘制适配 |
| u8log.c、u8log_u8g2.c、u8log_u8x8.c | 滚动日志和日志绘制 |
| u8x8_debounce.c | 按键去抖及 u8x8_GetMenuEvent() |
| u8x8_capture.c | 捕获 Tile、输出 PBM/XBM，主要用于调试/测试 |

注意：`u8x8_capture.c` 的 `u8x8_d_capture()`、`u8x8_ConnectCapture()` 等 Tile 捕获回调被
`NOT_YET_IMPLEMENTED_U8X8_SCREEN_CAPTURE` 包住，默认不会编译；常规 OLED 工程只需在确实要做屏幕抓取时再打开。

## 4. 当前支持的 SSD1306 128×32 I2C 用法

### 4.1 初始化和第一次绘图

下面的调用顺序与 u8g2_d_setup.c、u8x8_display.c 的实际逻辑一致：

调用链可以记成：`Setup` 绑定四类回调并建立显示信息 → `InitDisplay` 完成 GPIO/CAD/byte 初始化、复位和控制器 init → `ClearBuffer/DrawXXX` 改 RAM → `SendBuffer` 按 Tile 行交给 display/CAD/byte 回调发送。

~~~c
#include "u8g2.h"

static u8g2_t oled;

void oled_init(void)
{
    u8g2_Setup_ssd1306_i2c_128x32_univision_f(
        &oled,
        U8G2_R0,
        gd32_u8x8_byte_cb,             /* 应用提供 */
        gd32_u8x8_gpio_delay_cb);      /* 应用提供 */

    /*
     * CAD 层默认把地址设为 0x78（8 位写地址）。
     * 如果底层 HAL 使用 7 位地址，这里仍应把 0x3C 左移一位传入。
     * 该设置应在 InitDisplay 前完成。
     */
    u8g2_SetI2CAddress(&oled, (uint8_t)(0x3C << 1));

    u8g2_InitDisplay(&oled);           /* 初始化 GPIO、总线、复位并发送 init 序列 */
    u8g2_SetPowerSave(&oled, 0);       /* InitDisplay 后通常仍处于省电/关闭状态 */
}

void oled_show(void)
{
    u8g2_ClearBuffer(&oled);
    u8g2_SetFont(&oled, u8g2_font_5x8_tr);
    u8g2_DrawStr(&oled, 0, 8, "Hello u8g2");
    u8g2_DrawFrame(&oled, 0, 10, 128, 22);
    u8g2_SendBuffer(&oled);
}
~~~

u8g2_font_5x8_tr 在当前 u8g2_fonts.c 中存在。若不把字体数据编译进工程，会出现字体符号缺失；中文还必须选择包含目标汉字的字体，并使用 UTF-8 字符串配合 u8g2_DrawUTF8()。

当前显示信息是 128×32 像素、16×4 Tile；u8g2_Setup_ssd1306_i2c_128x32_univision_f() 固定组合了：

~~~text
display callback : u8x8_d_ssd1306_128x32_univision
CAD callback     : u8x8_cad_ssd13xx_fast_i2c
memory           : u8g2_m_16_4_f
low-level draw   : u8g2_ll_hvline_vertical_top_lsb
~~~

### 4.2 如果要使用 Winstar 或 SH1106 回调

本目录没有对应的高级 setup，但可以按 u8g2_d_setup.c 的方式手动拼装：

~~~c
u8g2_SetupDisplay(&oled,
                  u8x8_d_ssd1306_128x32_winstar, /* 或 u8x8_d_sh1106_128x32_visionox */
                  u8x8_cad_ssd13xx_fast_i2c,
                  gd32_u8x8_byte_cb,
                  gd32_u8x8_gpio_delay_cb);

uint8_t tile_rows;
uint8_t *buf = u8g2_m_16_4_f(&tile_rows);
u8g2_SetupBuffer(&oled, buf, tile_rows,
                 u8g2_ll_hvline_vertical_top_lsb, U8G2_R0);
~~~

Winstar 的显示信息带有 default_x_offset = 125，SH1106 visionox 带有 default_x_offset = 2；出现水平偏移时要先确认模块型号和回调是否匹配。

源码细节：`u8x8_d_sh1106_128x32_visionox()` 在处理 `U8X8_MSG_DISPLAY_INIT` 时会先发送
Visionox/SH1106 初始化序列，随后继续落入通用 SSD1306 处理路径，因此实际会再次执行通用复位并发送一遍 SSD1306 初始化序列。
这是当前裁剪代码的既有行为；若该模块初始化异常，优先用逻辑分析仪确认序列，或复制该回调并去掉第二次通用初始化。

## 5. GD32 必须提供的两个回调

回调类型在 u8x8.h 中定义为：

~~~c
typedef uint8_t (*u8x8_msg_cb)(
    u8x8_t *u8x8,
    uint8_t msg,
    uint8_t arg_int,
    void *arg_ptr);
~~~

回调返回 1 表示消息已处理，返回 0 表示不支持或失败；应用自己的 byte/GPIO 回调在成功完成每个消息后应返回 1。

### 5.1 byte callback：只负责总线事务和字节

以下 board_i2c_* 是项目 HAL 占位名，需要替换成 GD32 的硬件 I2C API：

~~~c
static uint8_t gd32_u8x8_byte_cb(
    u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_BYTE_INIT:
        board_i2c_init();
        return 1;

    case U8X8_MSG_BYTE_START_TRANSFER:
        board_i2c_start();
        /*
         * u8x8_GetI2CAddress() 返回“写地址”格式，当前默认是 0x78。
         * 若 board_i2c_send_address() 接收 7 位地址，请传
         * u8x8_GetI2CAddress(u8x8) >> 1。
         */
        board_i2c_send_address(u8x8_GetI2CAddress(u8x8));
        return 1;

    case U8X8_MSG_BYTE_SEND:
        /* arg_ptr 指向待发送数据，arg_int 是字节数；不要改写数据。 */
        board_i2c_write((const uint8_t *)arg_ptr, arg_int);
        return 1;

    case U8X8_MSG_BYTE_END_TRANSFER:
        board_i2c_stop();
        return 1;

    case U8X8_MSG_BYTE_SET_DC:
        /* I2C 没有 DC 线；SPI 方案才需要处理。 */
        return 1;

    default:
        return 0;
    }
}
~~~

fast I2C CAD 已经负责插入控制字：

- 命令：地址 -> 0x00 -> 命令/参数；
- 数据：地址 -> 0x40 -> 像素数据；
- 数据流会按不超过 24 字节拆分。

因此 byte callback 不要再次添加 0x00、0x40，也不要把 arg_ptr 当成带地址的完整帧。
常见 OLED 7 位地址为 `0x3C` 或 `0x3D`，对应本组件使用的 8 位写地址分别是 `0x78` 或 `0x7A`。

如果使用现成的软件总线，可把 u8x8_byte_sw_i2c() 或 u8x8_byte_4wire_sw_spi() 直接作为 byte callback，但 GPIO/delay callback 还必须实现
`U8X8_MSG_GPIO_I2C_CLOCK`、`U8X8_MSG_GPIO_I2C_DATA`，或 SPI 的
`U8X8_MSG_GPIO_SPI_CLOCK`、`U8X8_MSG_GPIO_SPI_DATA` 等消息。

### 5.2 GPIO/delay callback：复位、控制线和延时

~~~c
static uint8_t gd32_u8x8_gpio_delay_cb(
    u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    (void)arg_ptr;

    switch (msg)
    {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        board_oled_gpio_init();
        return 1;

    case U8X8_MSG_GPIO_RESET:
        board_oled_reset_write(arg_int);
        return 1;

    case U8X8_MSG_GPIO_DC:             /* SPI/并行时使用 */
        board_oled_dc_write(arg_int);
        return 1;

    case U8X8_MSG_GPIO_CS:             /* SPI/并行时使用 */
        board_oled_cs_write(arg_int);
        return 1;

    case U8X8_MSG_DELAY_MILLI:
        board_delay_ms(arg_int);
        return 1;

    case U8X8_MSG_DELAY_10MICRO:
    case U8X8_MSG_DELAY_100NANO:
    case U8X8_MSG_DELAY_NANO:
    case U8X8_MSG_DELAY_I2C:
        board_delay_short(msg, arg_int); /* 硬件 I2C 可按需要空操作 */
        return 1;

    default:
        return 0;
    }
}
~~~

SSD1306 的显示回调会通过 u8x8_d_helper_display_init() 调用 GPIO 初始化、CAD 初始化和 RESET 时序；当前显示信息给出的复位脉冲和复位后等待时间都是 100 ms。因此不能只实现 I2C 写函数而忽略 RESET/毫秒延时。
按当前 helper 的实际顺序，RESET 电平是 `1 → 延时 100 ms → 0 → 延时 100 ms → 1 → 延时 100 ms`；回调中的 `arg_int` 是要输出的逻辑电平，具体高/低有效由你的模块连线决定。

## 6. 缓冲、刷新和坐标

### 6.1 全屏缓冲（当前默认模式）

当前唯一的高级 SSD1306 setup 使用 _f 全屏缓冲，固定占用 512 字节：

~~~c
u8g2_ClearBuffer(&oled);       /* 清 RAM 缓冲，不会立即清屏 */
/* 所有 DrawXXX 都写入 RAM */
u8g2_SendBuffer(&oled);        /* 发送到控制器 */
~~~

相关 API：

| API | 作用 |
|---|---|
| u8g2_ClearBuffer() | 清空当前 RAM 缓冲 |
| u8g2_SendBuffer() | 发送缓冲，并调用 u8x8_RefreshDisplay() |
| u8g2_UpdateDisplay() | 发送缓冲，但不额外发送 refresh 消息 |
| u8g2_UpdateDisplayArea(u8g2, tx, ty, tw, th) | 只更新指定 Tile 区域 |

u8g2_UpdateDisplayArea() 只在全屏缓冲下工作；参数是 Tile 坐标和尺寸，不是像素坐标；它忽略旋转，也不会触发电子纸 refresh。

### 6.2 分页缓冲

头文件和 u8g2_buffer.c 仍提供分页 API：

~~~c
u8g2_FirstPage(&oled);
do
{
    u8g2_DrawStr(&oled, 0, 8, "page buffer");
    u8g2_DrawFrame(&oled, 0, 10, 128, 22);
} while (u8g2_NextPage(&oled));
~~~

u8g2_FirstPage() 默认会自动清空缓冲；u8g2_NextPage() 发送当前页，返回 1 表示还有下一页，最后一页返回 0。当前裁剪版没有 _1/_2 高级 setup；只有在自行调用 u8g2_SetupBuffer()、提供较小的 Tile 缓冲后，才应使用这个循环。

### 6.3 像素坐标和 Tile 坐标

| 层 | 坐标单位 | 128×32 示例 |
|---|---|---|
| u8g2 绘图 | 像素 | x=0..127、y=0..31 |
| u8x8 Tile | 8×8 块 | x=0..15、y=0..3 |

### 6.4 多实例和动态缓冲

u8g2_m_16_4_f() 内部返回静态 buf[512]。多个 u8g2_t 通过默认 setup 初始化时会共享这块 RAM，任一实例绘图都会改写同一块缓冲。需要多实例时，建议在整个工程统一定义 U8G2_USE_DYNAMIC_ALLOC，并为每个实例提供独立缓冲：

~~~c
#define U8G2_USE_DYNAMIC_ALLOC
#include "u8g2.h"

static uint8_t oled_buf[512];

/* setup 后、任何绘图前绑定自己的缓冲 */
u8g2_SetBufferPtr(&oled, oled_buf);
~~~

动态模式下 u8g2_m_16_4_f() 返回空指针，必须先绑定缓冲；编译宏要在组件源文件和应用工程中保持一致。另有两个需要注意的共享状态：多边形实现使用全局 u8g2_pg，fast I2C CAD 使用静态 in_transfer，因此不要让多个任务/中断并发操作显示对象。

## 7. u8g2 常用 API

### 初始化和显示控制

~~~c
u8g2_InitInterface(&oled);             /* 只初始化接口 */
u8g2_InitDisplay(&oled);               /* 初始化接口 + 复位 + init 序列 */
u8g2_SetPowerSave(&oled, 0);           /* 0=正常显示，1=省电 */
u8g2_SetFlipMode(&oled, 0);             /* 0=正常，1=硬件 180° 翻转 */
u8g2_SetContrast(&oled, value);         /* 0..255，实际效果依控制器 */
u8g2_SetDisplayRotation(&oled, U8G2_R0);
u8g2_SetI2CAddress(&oled, address);
u8g2_GetI2CAddress(&oled);
u8g2_GetDisplayWidth(&oled);
u8g2_GetDisplayHeight(&oled);
u8g2_ClearDisplay(&oled);               /* 通过分页循环可靠清控制器 */
~~~

InitInterface() 和 InitDisplay() 是二选一；源码明确说明 InitDisplay() 内部已经完成接口初始化，不要连续调用两者。InitDisplay() 后通常还要 SetPowerSave(0)。

### 像素、线和几何图形

~~~c
u8g2_SetDrawColor(&oled, 1);             /* 0=清除，1=设置，2=XOR；>=3 会被当作 1 */
u8g2_SetClipWindow(&oled, x0, y0, x1, y1);
u8g2_SetMaxClipWindow(&oled);

u8g2_DrawPixel(&oled, x, y);
u8g2_DrawHLine(&oled, x, y, len);
u8g2_DrawVLine(&oled, x, y, len);
u8g2_DrawLine(&oled, x1, y1, x2, y2);
u8g2_DrawHVLine(&oled, x, y, len, dir);

u8g2_DrawBox(&oled, x, y, w, h);
u8g2_DrawFrame(&oled, x, y, w, h);
u8g2_DrawRBox(&oled, x, y, w, h, radius);
u8g2_DrawRFrame(&oled, x, y, w, h, radius);

u8g2_DrawCircle(&oled, cx, cy, radius, U8G2_DRAW_ALL);
u8g2_DrawDisc(&oled, cx, cy, radius, U8G2_DRAW_ALL);
u8g2_DrawEllipse(&oled, cx, cy, rx, ry, U8G2_DRAW_ALL);
u8g2_DrawFilledEllipse(&oled, cx, cy, rx, ry, U8G2_DRAW_ALL);
u8g2_DrawArc(&oled, cx, cy, radius, start, end);
u8g2_DrawTriangle(&oled, x0, y0, x1, y1, x2, y2);
~~~

圆和椭圆的 option 可使用 U8G2_DRAW_UPPER_RIGHT、U8G2_DRAW_UPPER_LEFT、U8G2_DRAW_LOWER_LEFT、U8G2_DRAW_LOWER_RIGHT 或 U8G2_DRAW_ALL。
`u8g2_DrawArc()` 的 `start/end` 是 0..255 的整圆角度刻度（不是度数）；源码中 `start == end` 表示整圆。

按钮和多边形接口：

~~~c
u8g2_DrawButtonFrame(&oled, x, y, flags, text_width, padding_h, padding_v);
u8g2_DrawButtonUTF8(&oled, x, y, flags, width, padding_h, padding_v, text);

u8g2_ClearPolygonXY();
u8g2_AddPolygonXY(&oled, x, y);   /* 当前实现最多保留 6 个点 */
u8g2_DrawPolygon(&oled);
~~~

按钮 `flags` 可组合 `U8G2_BTN_BW0..U8G2_BTN_BW3`、`U8G2_BTN_SHADOW0..U8G2_BTN_SHADOW2`、`U8G2_BTN_INV`、`U8G2_BTN_HCENTER` 和 `U8G2_BTN_XFRAME`。

### 字体和文字

~~~c
u8g2_SetFont(&oled, u8g2_font_5x8_tr);
u8g2_SetFontMode(&oled, 1);       /* 1=透明背景，0=同时绘制背景 */
u8g2_SetFontDirection(&oled, 0);  /* 0..3 */

u8g2_DrawStr(&oled, x, y, "ASCII");
u8g2_DrawUTF8(&oled, x, y, "UTF-8 文本");
u8g2_DrawGlyph(&oled, x, y, encoding);
u8g2_DrawStrX2(&oled, x, y, text);
u8g2_DrawUTF8X2(&oled, x, y, text);

u8g2_GetStrWidth(&oled, text);
u8g2_GetUTF8Width(&oled, text);
u8g2_IsGlyph(&oled, encoding);
u8g2_IsAllValidUTF8(&oled, text);
u8g2_GetAscent(&oled);
u8g2_GetDescent(&oled);

u8g2_SetFontPosBaseline(&oled);
u8g2_SetFontPosBottom(&oled);
u8g2_SetFontPosTop(&oled);
u8g2_SetFontPosCenter(&oled);
~~~

文字的 y 默认是基线位置；需要顶部对齐时显式调用 u8g2_SetFontPosTop()。u8g2_DrawStr() 按普通单字节字符串处理，中文应使用 UTF-8 编码、包含目标字形的字体和 u8g2_DrawUTF8()。
`DrawStr/DrawUTF8` 返回本次绘制后的前进宽度，`GetStrWidth/GetUTF8Width` 返回测量宽度，可用于右对齐或居中。

### 位图

~~~c
u8g2_SetBitmapMode(&oled, 1);       /* 1=位图中的 0 位透明 */
u8g2_DrawBitmap(&oled, x, y, cnt, h, bitmap);
u8g2_DrawXBM(&oled, x, y, w, h, bitmap);
u8g2_DrawXBMP(&oled, x, y, w, h, bitmap);
u8g2_DrawHorizontalBitmap(&oled, x, y, len, bitmap);
~~~

普通 Bitmap、XBM 和 XBMP 的位顺序不同，数组要按对应格式生成。u8g2_DrawXBMP() 的注释说明它假设位图位于程序存储区；在 GD32 上通常应使用 const 数据并确认链接器存放位置。
其中 `u8g2_DrawBitmap()` 的 `cnt` 是每行字节数（实际宽度为 `cnt×8`），而 `DrawXBM/DrawXBMP` 的宽高直接按像素传入。

## 8. u8x8 轻量接口

本目录没有控制器专用的 u8x8 便捷 setup（例如 128×64 I2C OLED 的预组合函数）。必须显式传入五个回调：

~~~c
u8x8_t u8x8;

u8x8_Setup(&u8x8,
           u8x8_d_ssd1306_128x32_univision,
           u8x8_cad_ssd13xx_fast_i2c,
           gd32_u8x8_byte_cb,
           gd32_u8x8_gpio_delay_cb);

u8x8_SetI2CAddress(&u8x8, 0x78);
u8x8_InitDisplay(&u8x8);
u8x8_SetPowerSave(&u8x8, 0);
u8x8_SetFont(&u8x8, u8x8_font_5x8_r);
u8x8_DrawString(&u8x8, 0, 0, "Hello");
~~~

常用接口：

| API | 作用 |
|---|---|
| u8x8_Setup() | 绑定 display/CAD/byte/GPIO 四类回调 |
| u8x8_InitInterface() | 只初始化接口，不复位、不发 init 序列 |
| u8x8_InitDisplay() | 初始化接口、复位并发送控制器 init 序列 |
| u8x8_SetPowerSave()、u8x8_SetFlipMode()、u8x8_SetContrast() | 显示状态控制 |
| u8x8_ClearDisplay()、u8x8_FillDisplay()、u8x8_ClearLine() | 直接按 Tile 清屏/填充/清行 |
| u8x8_DrawTile() | 写入 Tile，参数是 Tile 坐标和 Tile 数量 |
| u8x8_SetFont()、u8x8_DrawString()、u8x8_DrawUTF8() | 字符绘制，返回绘制的字形数 |
| u8x8_GetCols()、u8x8_GetRows() | 获取 Tile 列数和行数 |
| u8x8_SetI2CAddress()、u8x8_GetI2CAddress() | 设置/查询写地址 |
| u8x8_GetMenuEvent() | 读取并去抖菜单按键 |

`u8x8_GetMenuEvent()` 的输入不是通过函数参数传入的：`u8x8_debounce.c` 会逐个调用
`gpio_and_delay_cb` 的 `U8X8_MSG_GPIO(pin)`，回调必须把实际电平写入
`u8x8->gpio_result`（也可调用 `u8x8_SetGPIOResult()`）。默认按键状态为 1，检测到稳定的按下→释放后才返回
`U8X8_MSG_GPIO_MENU_*` 事件；因此菜单轮询需要周期性调用，不能只读一次。

u8x8_InitDisplay() 后同样要调用 u8x8_SetPowerSave(&u8x8, 0)。u8x8_RefreshDisplay() 主要为需要额外刷新命令的电子纸控制器保留，SSD1306 通常不需要。
`u8x8_DrawTile()` 的 `tile_ptr` 每个 Tile 占 8 字节；当前 SSD1306/u8g2 组合使用 vertical-top-LSB 布局，直接拼 Tile 数据时要按这个位序生成。

## 9. MUI 菜单/表单框架

MUI 不是 RTOS，也不是完整窗口系统；它把表单字符串、字段描述符和字段回调组合起来。u8g2 适配在 mui_u8g2.h/.c。

核心初始化签名必须按 mui.h 使用：

~~~c
void mui_Init(mui_t *ui,
              void *graphics_data,
              fds_t *fds,
              muif_t *muif_tlist,
              size_t muif_tcnt);
~~~

其中 graphics_data 在 u8g2 适配中应指向 u8g2_t。mui_Init() 只建立菜单系统，不会自动进入或绘制表单；典型顺序是：

~~~c
#include "mui_u8g2.h"

static mui_t ui;

static fds_t forms[] MUI_PROGMEM =
    MUI_FORM(1)
    MUI_STYLE(0)
    MUI_LABEL(0, 8, "Settings");

static muif_t fields[] MUI_PROGMEM =
{
    MUIF_U8G2_LABEL(),
    MUIF_U8G2_FONT_STYLE(0, u8g2_font_5x8_tr)
};

mui_Init(&ui, &oled, forms, fields,
         sizeof(fields) / sizeof(fields[0]));
mui_GotoForm(&ui, 1, 0);       /* 或 mui_EnterForm(&ui, form_ptr, 0) */

/* UI 任务中循环 */
u8g2_ClearBuffer(&oled);
mui_Draw(&ui);
u8g2_SendBuffer(&oled);
~~~

常用表单宏有 MUI_FORM()、MUI_STYLE()、MUI_DATA()、MUI_XY()、MUI_XYT()、MUI_XYA()、MUI_XYAT()、MUI_LABEL() 和 MUI_GOTO()。常用事件函数有：

~~~c
mui_NextField(&ui);
mui_PrevField(&ui);
mui_SendSelect(&ui);
mui_SendSelectWithExecuteOnSelectFieldSearch(&ui);
mui_SendValueIncrement(&ui);
mui_SendValueDecrement(&ui);
mui_SaveForm(&ui);
mui_RestoreForm(&ui);
mui_GetCurrentFormId(&ui);
~~~

字段回调使用 MUIF_MSG_DRAW、MUIF_MSG_CURSOR_SELECT、MUIF_MSG_VALUE_INCREMENT 等消息；具体字段类型和 MUIF_U8G2_* 宏以 mui_u8g2.h 为准。MUI 的字段和表单状态是普通内存，不要在多个任务中并发操作。

## 10. u8log 滚动日志

u8log_Init() 的真实参数顺序是“对象、宽度、高度、缓冲区”，日志缓冲区大小至少为 width × height 字节：

~~~c
static u8log_t log;
static uint8_t log_buf[16 * 4];

u8log_Init(&log, 16, 4, log_buf);
u8log_WriteString(&log, "temp=");
u8log_WriteDec16(&log, temperature, 5);
u8log_WriteString(&log, "\n");
u8log_WriteString(&log, "status=ok\n");

u8g2_SetFont(&oled, u8g2_font_5x8_tr);
u8g2_DrawLog(&oled, 0, u8g2_GetAscent(&oled), &log);
~~~

u8log_WriteString() 只有两个参数，不支持 printf 格式化；日志实现会处理 `\n`、`\r`、`\t`、`\f` 控制字符。数值请使用 u8log_WriteDec8()、u8log_WriteDec16()、u8log_WriteHex8/16/32()，或者先用 snprintf() 格式化后再写入；`WriteDec8` 的位数参数建议为 1..3，`WriteDec16` 为 1..5。需要自动重绘时可使用：

~~~c
u8log_SetCallback(&log, u8log_u8g2_cb, &oled);
u8log_SetRedrawMode(&log, 1);
u8log_SetLineHeightOffset(&log, 0);
~~~

u8log_u8g2_cb() 内部会自己执行 FirstPage/NextPage；如果不设置回调，则把 u8g2_DrawLog() 放进应用自己的绘图流程。
该回调默认会清空并重绘整个 u8g2 缓冲区，若日志要和其它图形合成到同一页，建议不要启用自动回调，而是在自己的绘图流程中调用 u8g2_DrawLog()。

## 11. 编译和集成

当前 MDK-ARM/template.uvprojx 没有引用 Packages/u8g2；文件放在目录里不等于已经加入 Keil 工程。至少要把 include path 和对应源文件加入工程组。

当前 SSD1306 u8g2 核心涉及：

~~~text
u8g2_d_setup.c       u8g2_d_memory.c
u8g2_setup.c         u8g2_buffer.c
u8g2_cleardisplay.c  u8g2_font.c
u8g2_hvline.c        u8g2_ll_hvline.c
u8g2_intersection.c  u8g2_kerning.c
u8x8_setup.c         u8x8_display.c
u8x8_cad.c           u8x8_byte.c
u8x8_gpio.c          u8x8_8x8.c
u8x8_string.c        u8x8_u8toa.c
u8x8_u16toa.c
u8x8_d_ssd1306_128x32.c
~~~

按实际 API 再加入 u8g2_line.c、u8g2_box.c、u8g2_circle.c、u8g2_arc.c、u8g2_polygon.c、u8g2_bitmap.c、MUI/u8log/UI 文件。
使用 u8g2/u8x8 选择列表、消息框或数值输入时，还要加入对应的 UI 源文件和 `u8x8_debounce.c`；使用 u8x8 字体时加入 u8x8_fonts.c；使用 u8g2 字体时加入包含目标字体数组的源文件。
`u8g2_buffer.c` 中的 PBM/XBM 导出函数调用 `u8x8_capture_*`；若链接器没有函数级垃圾回收，建议同时加入 `u8x8_capture.c`，否则可能出现未解析符号。

特别注意：

- u8g2_fonts.c 约 37.7 MB，u8x8_fonts.c 约 1.6 MB；不要无选择地把整套字库放入 GD32 Flash。
- 当前字库是大文件集合，建议只保留需要的字体数组，或确认编译器/链接器启用了数据段和函数级垃圾回收。
- u8g2.h 强制定义 U8G2_16BIT；Unicode、字体旋转、裁剪窗口和相交检查则是默认启用（可用对应的 `U8G2_WITHOUT_*` 宏关闭）。这些开关会影响代码体积，也决定可用 API。
- U8X8_WITH_USER_PTR 默认未定义；u8g2_SetUserPtr()、u8x8_SetUserPtr() 只有在工程统一定义该宏后才存在，不能只在单个应用文件中临时定义。
- 回调是同步契约。若底层改成 DMA，必须保证 arg_ptr 指向的缓冲在传输完成前不被复用，并且 START/END 事务边界不被打断。

## 12. 常见问题定位

1. **链接不到 setup**：先确认调用的是本目录真正实现的 u8g2_Setup_ssd1306_i2c_128x32_univision_f()，并已加入 u8g2_d_setup.c。
2. **屏幕完全不亮**：检查回调是否收到 BYTE_INIT、BYTE_START_TRANSFER、BYTE_SEND、BYTE_END_TRANSFER；再查 RESET、毫秒延时、I2C 地址和 SetPowerSave(0)。
3. **I2C 有地址但无图像**：确认传给 HAL 的地址格式。CAD 默认是 8 位写地址 0x78，常见 7 位地址是 0x3C。
4. **只显示一部分**：全屏模式必须调用 u8g2_SendBuffer()；手动分页模式必须把全部绘图放进 FirstPage() / NextPage() 循环。
5. **图像横向偏移**：确认 univision/winstar/visionox 回调和实际模块匹配；这些回调的 x offset 不同。
6. **中文乱码或空白**：字符串必须是 UTF-8，所选字体必须包含对应字形，并调用 u8g2_DrawUTF8()。
7. **RAM 被覆盖**：默认 u8g2_m_16_4_f() 是共享的 512 字节静态缓冲；多实例请启用动态缓冲或自行绑定独立内存。
8. **MUI/u8g2 内置对话框卡死**：u8g2_UserInterfaceSelectionList()、u8g2_UserInterfaceMessage()、u8g2_UserInterfaceInputValue() 都是阻塞式循环，依赖 u8x8_GetMenuEvent() 和菜单 GPIO 回调，不要在中断中调用。
9. **日志调用编译报错**：u8log_WriteString() 不接受格式化参数；请先格式化字符串，或使用 u8log_WriteDec*() / u8log_WriteHex*()。
10. **改了地址仍无效**：地址设置要作用于同一个 u8g2_t/u8x8_t 实例，并在下一次传输前生效；不要在 byte callback 中重复覆盖它。

## 13. 源码核对结论

本说明重点核对了以下实现，而不是只看声明：

- u8g2_d_setup.c：确认当前高级 setup 的显示回调、CAD、缓冲和底层绘图布局。
- u8g2_d_memory.c：确认唯一保留的 512 字节静态缓冲和 U8G2_USE_DYNAMIC_ALLOC 行为。
- u8x8_d_ssd1306_128x32.c：确认三个实际显示回调、分辨率、Tile 数、x offset，以及 Visionox 初始化路径的既有二次序列行为。
- u8x8_cad.c：确认 I2C 地址默认值、0x00/0x40 控制字节和 24 字节分块。
- u8x8_display.c、u8g2_buffer.c：确认初始化、复位、省电、全屏发送、分页和局部刷新语义。
- u8x8_debounce.c：确认菜单输入通过 `gpio_result` 回传，并在稳定按下/释放后产生事件。
- u8x8_string.c、u8x8_u8toa.c、u8x8_u16toa.c：确认字符串绘制和日志十进制转换的链接依赖。
- mui.h、mui_u8g2.h：确认 MUI 初始化签名、表单/事件 API 和 u8g2 适配方式。
- u8x8.h、u8log.c、u8log_u8g2.c：确认日志参数顺序、数值输出接口和日志绘制流程。

最终以本目录的 u8g2.h、u8x8.h 及实际参与编译的 .c 文件为准；如果后续把完整版控制器驱动或字体加入工程，应同步更新这份支持矩阵。
