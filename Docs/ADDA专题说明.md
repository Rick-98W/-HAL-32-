# ADDA 专题说明：ADC / DAC / 波形分析 / 串口导出 是怎么运作的

这份文档只讲 ADDA 这一条线：DAC 输出 → ADC 采样 → 波形分析 / 串口导出。重点是：

* 每个模块**拥有**哪些状态（标志位、缓冲区）；
* 状态**在哪里被改、被谁读**；
* 跨模块时**指针和回调**是怎么传的；
* 一次完整操作**按什么顺序**发生。

全局的模块关系、菜单、指令格式见 [业务说明.md](业务说明.md) 和 [串口指令集.md](串口指令集.md)，这里不重复。

---

## 0. 一分钟版本

1. DAC 输出一个周期 256 步的波形，TIM6 决定每步多久。
2. TIM3 按**同样的 ARR**（double 时减半）触发 ADC，ADC 的 DMA 把结果循环写进 `s_ADCWaveWindow[1024]`。
3. 窗口里有 1024 个点（double）或 512 个点（single），按设计约是**两个周期**。
4. 分析和导出都**不直接读那个窗口**，而是先拷一份快照，因为 DMA 一直在写。
5. DAC 一改设置，ADC 就**重置窗口**并把**代数（generation）加一**。分析结果记住了它当时的代数，代数对不上就视为过期。
6. 导出用的快照要活很久（跨很多个主循环），所以由**发起导出的那一层**持有并释放；分析的快照用完即释放。

---

## 1. 谁拥有什么（最重要的一张表）

"拥有"的意思是：这块内存或这个标志**只有这个模块能直接写**。其他模块只能调用它的函数。

| 状态 | 定义在 | 拥有者 | 谁写 | 谁读 |
| :--- | :--- | :--- | :--- | :--- |
| `s_ADCWaveWindow[1024]` | `ADC_service.c` | ADC | DMA（硬件）| 只有 `ADC_snapshot` 读 |
| `isDoubled` | `ADC_service.c` | ADC | `ADC_set_ratio` | `ADC_get_ratio`、`ADC_get_window_len`、`ADC_set_TIM3_ARR` |
| `s_dacARR` | `ADC_service.c` | ADC | `ADC_on_DAC_change` | `ADC_set_TIM3_ARR` |
| `s_wrapped` | `ADC_service.c` | ADC | 中断回调 `HAL_ADC_ConvCpltCallback`，`ADC_reset_window` 清零 | `ADC_is_window_ready` |
| `s_generation` | `ADC_service.c` | ADC | `ADC_reset_window` | `ADC_get_generation` |
| `s_inited` | `ADC_service.c` | ADC | `ADC_init` 末尾 | `ADC_on_DAC_change` |
| `s_info` | `wavAnl_service.c` | wavAnl | `wavAnl_update` | `wavAnl_get_*`、`wavAnl_menu_info` |
| `s_has_result` | `wavAnl_service.c` | wavAnl | `wavAnl_update` | `wavAnl_is_valid` |
| `s_resultGen` | `wavAnl_service.c` | wavAnl | `wavAnl_update` | `wavAnl_is_valid` |
| `s_busy` 等 | `serial_export_task.c` | 串口导出 | `serial_export_start` / `serial_export_proc` | `serial_export_busy` |
| `s_exportSnap` | `menu_ADC_suppl.c` | 导出适配层 | `ADC_wave_export_start` / `ADC_export_release` | 同文件 |
| `targetARR` | `DAC_service.c` | DAC | `DAC_set_waveform_period_ms` | `DAC_register_change_cb` 回放、`DAC_notify_change` |
| `s_changeCb` | `DAC_service.c` | DAC | `DAC_register_change_cb` | `DAC_notify_change` |

注意两点：

* `static` 变量在文件外**看不见**，这就是"不透明"的最基本形式。`s_wrapped` 在 `ADC_service.c` 外面根本没有名字，别的模块想知道窗口好不好，只能调用 `ADC_is_window_ready()`。
* `s_ADCWaveWindow` 没有 `static`，是唯一的例外。全工程 grep 结果显示它只在 `ADC_service.c` 里被使用，所以加上 `static` 不会影响别的文件。这是一个值得做的小修改。

---

## 2. 标志位与状态详解

### 2.1 ADC 窗口的两个"好了"

这是最容易混淆的地方，代码里有两种判断同时存在：

```c
bool ADC_is_window_ready(void) {
    return s_wrapped || ADC_write_index() >= ADC_get_window_len();
}
```

| 条件 | 含义 | 什么时候为真 |
| :--- | :--- | :--- |
| `ADC_write_index() >= len` | **这一轮**已经写够了 `len` 个点 | 复位之后，DMA 从 0 开始写，写到 `len` 个点时为真 |
| `s_wrapped` | DMA **转满过一整圈**（1024 个点）至少一次 | DMA 循环模式每写满 1024 个点，中断回调设一次 |

`s_wrapped` 的作用是"整圈都有效"的快捷判断：DMA 转满过一圈后，缓冲区的每个位置都写过至少一次，任何长度的窗口都有完整数据，不需要再看写指针。`ADC_is_window_ready` 之所以两个都看，是因为复位之后、第一次转满之前，只能靠写指针判断。`s_wrapped` 只在 `ADC_reset_window` 里清零。

### 2.2 写指针怎么算

```c
static uint32_t ADC_write_index(void) {
    return (ADC_WINDOW_SIZE - __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle)) % ADC_WINDOW_SIZE;
}
```

DMA 的"剩余计数"是**倒着**数的：从 1024 减到 0。所以"已写入的个数" = `1024 - 剩余计数`，就是下一个要写的位置。`% 1024` 处理计数到 0 时的回绕。

### 2.3 代数（generation）

`s_generation` 是一个只增不减的计数器，每次窗口被重置加一。它**不记录设置是什么**，只记录"窗口换过几次"。

为什么比逐个比较设置更可靠？因为"设置变了又变回去"时，设置值一样，但窗口里的数据已经是另一套设置采的了。代数会变，设置值不会，所以代数能发现这种情况。

### 2.4 `isDoubled` 与 TIM3

`ADC_set_TIM3_ARR()`：

```c
uint32_t ARR = s_dacARR;
if (isDoubled) ARR /= 2;
```

* TIM3 和 TIM6 的预分频都是 `30-1`（`Core/Src/tim.c`），时钟一致。
* 所以 TIM3 用同样的 ARR，就是每个 DAC 步触发一次采样。一个周期 256 步，得到 256 个点。single 窗口 512 点，约两个周期。
* double 时 ARR 减半，采样快一倍，同样两个周期得到 1024 点。

这是按代码推算的结论，不是测量结果。

### 2.5 波形分析结果的标志

`wavAnl_service.c` 里有三个变量：

| 变量 | 含义 |
| :--- | :--- |
| `s_has_result` | 成功分析过至少一次。失败不会清它 |
| `s_resultGen` | 分析那一刻的 `ADC_get_generation()` |
| `s_info` | 分析库返回的结果结构体（频率、周期、峰峰值、类型等） |

有效性只有一个判断：

```c
bool wavAnl_is_valid(void) {
    return s_has_result && s_resultGen == ADC_get_generation();
}
```

结果在内存里可能还在（`s_info` 没被清），但代数对不上，就是**过期**。调用方要通过 `wavAnl_is_valid()` 决定用不用它。

### 2.6 导出的状态

导出用两组变量，分属两个文件：

| 文件 | 变量 | 作用 |
| :--- | :--- | :--- |
| `serial_export_task.c` | `s_busy` | 正在导出，`true` 到 `done` 调用为止 |
| | `s_src` `s_ctx` | 数据来源：函数指针 + 指向数据的指针 |
| | `s_count` | 一共几行 |
| | `s_idx` | 下一行的编号 |
| | `s_done` | 导出结束时调用的函数（可以为空） |
| `menu_ADC_suppl.c` | `s_exportSnap` | 快照内存，导出期间有效。`NULL` 表示没有快照 |

`s_busy` 是通用模块自己的；`s_exportSnap` 是应用层的。两者通过 `s_ctx` 和 `done` 连起来，见第 4.4 节。

---

## 3. 指针与回调：为什么 `serial_export` 不认识 ADC

这一节专门讲你最困惑的部分。以 `serial_export` 为例。

### 3.1 三样东西

```c
typedef uint16_t (*serial_export_src_t)(void *ctx, uint32_t index);
typedef void (*serial_export_done_t)(void);

bool serial_export_start(serial_export_src_t src, void *ctx, uint32_t count, serial_export_done_t on_done);
```

| 参数 | 类型 | 含义 | 类比 |
| :--- | :--- | :--- | :--- |
| `src` | 函数指针 | "给我第 `index` 个值的函数" | 一个"读取器" |
| `ctx` | `void *` | 一个**不透明**的指针，原样交给 `src` | 一把钥匙，只有 `src` 知道怎么用 |
| `on_done` | 函数指针 | 导出结束时调用 | 完成通知 |

`serial_export` 只负责**按顺序调用 `src`，把返回值打印出来**。它不知道数据是 `uint16_t` 数组、是 DAC 的表、还是别的什么。

### 3.2 `void *` 是擦除了类型的指针

`ctx` 的类型是 `void *`，意思是"某个地址，类型我不关心"。`serial_export` 只把它存起来、再传回去，从不解引用。

真正知道类型的是 `src`，在 `menu_ADC_suppl.c` 里：

```c
static uint16_t ADC_export_sample(void *ctx, uint32_t index) {
    const uint16_t *snap = ctx;                     // 把 void* 还原成真实类型
    return (uint16_t)ADC_raw_to_mv(snap[index]);    // 读第 index 个点并换算成 mV
}
```

`const uint16_t *snap = ctx;` 是 C 的隐式转换：`void *` 可以直接赋给任何指针类型。

### 3.3 同样的模式出现在分析库里

`wavAnl_service.c` 调用分析库时用的是同一个模式：

```c
static float ADC_window_sample(void *ctx, uint32_t index) {
    const uint16_t *snap = ctx;
    return (float)ADC_raw_to_mv(snap[index]) * 0.001f;
}

s_info = wave_analyze(ADC_window_sample, snap, len, wave_sample_interval_s());
```

分析库 `waveform_analyzer` 不知道 ADC 的存在，它只要求"给我一个读取器和一个 ctx"。这就是为什么分析库可以放在 `Packages/` 里独立复用。

### 3.4 生命周期：谁负责释放

指针指向的内存必须**活得比使用它的人长**。这里有两种情况：

| 场景 | 快照活多久 | 谁分配 | 谁释放 |
| :--- | :--- | :--- | :--- |
| 波形分析 `wavAnl_update` | 同一个函数内，分析完就没用了 | 同函数 `MP_alloc` | 同函数 `MP_free` |
| 串口导出 `ADC_wave_export_start` | 跨很多个主循环（1024 行 × 1 ms） | 同函数 `MP_alloc` | 导出结束的 `done` 回调，或启动失败时立即释放 |

导出的快照**不能**在 `ADC_wave_export_start` 返回前释放，否则 `serial_export_proc` 下一次读就是野指针。所以它被存进了 `s_exportSnap`，在 `ADC_export_release` 里统一释放。

记住这个规则：**同步用完就放；异步要活很久，就由发起方放到它的回调里。**

---

## 4. 业务流程（按顺序看）

### 4.1 DAC 改设置 → ADC 窗口重置

以串口 `DAC set period 2000` 为例：

```
USART_parse_command
  └─ DAC_dispatch_command
       └─ DAC_set_period(2000)                          (DAC_service.c)
            ├─ 夹紧到 [5, 5400]，与当前值比较
            ├─ 值变了：s_currPeriod = 2000
            ├─ DAC_set_waveform_period_ms(2000)  → 算出新 targetARR，写 TIM6
            └─ DAC_restart_dma()
                 ├─ DAC_notify_change()  → s_changeCb(targetARR)
                 │      └─ ADC_on_DAC_change(ARR)     (ADC_service.c)
                 │           ├─ s_dacARR = ARR
                 │           ├─ ADC_set_TIM3_ARR()    → TIM3 ARR 更新（double 减半）
                 │           └─ s_inited 为真 → ADC_reset_window()
                 │                ├─ HAL_ADC_Stop_DMA
                 │                ├─ s_wrapped = false
                 │                ├─ s_generation++      ← 旧结果从此过期
                 │                └─ HAL_ADC_Start_DMA   ← 从头开始写
                 └─ 停止并重启 DAC 的 DMA 和 TIM6
```

**结果**：窗口里没有数据（`ADC_is_window_ready()` 为假），代数加一（之前的分析结果变为无效）。之后要等 DMA 重新写满，才能再次分析。

几个值得注意的地方：

* FSR 改变也会走同样的路（`DAC_set_FSR` 直接通知），因为幅度变了，旧窗口同样作废。
* 值没变就**不通知**，窗口和代数都不动。
* `DAC_init` 在 `ADC_init` 之前，`DAC_register_change_cb` 注册时会立即回放一次当前 ARR，这时 `s_inited` 还是假，所以只设 TIM3，不重置窗口。

### 4.2 串口 `ADC analyze`

```
USART_parse_command
  └─ ADC_dispatch_command
       └─ wavAnl_update()                                  (wavAnl_service.c)
            ├─ ADC_is_window_ready()?  否 → 打印 "window not full"，返回 false
            ├─ MP_alloc(len * 2)       → snap（失败则返回 false）
            ├─ ADC_snapshot(snap, len)                     (ADC_service.c)
            │    ├─ HAL_TIM_Base_Stop(TIM3)                 ← 暂停采样
            │    ├─ 按时间顺序拷贝 len 个点到 snap
            │    ├─ 检查拷贝期间 DMA 写指针是否前进过多，过多则重试
            │    └─ HAL_TIM_Base_Start(TIM3)                ← 恢复采样
            ├─ wave_analyze(ADC_window_sample, snap, len, 采样间隔)
            ├─ MP_free(snap)                               ← 快照用完即放
            ├─ s_has_result = true
            └─ s_resultGen = ADC_get_generation()           ← 记下"这是哪一代窗口的结果"
       ↓ 返回 true
       ├─ 打印 "[CMD] ADC wave analyzed"
       └─ ADC_menu_refresh_wave()    → 只有菜单在 WAVE_PARAM 模式时才 OLED_proc()
```

`ADC_snapshot` 的重试是为了应对 TIM3 停止之前已经在途的那一次转换。TIM3 暂停后，正常情况下不会再有新数据进来，所以大多数时候第一次就成功。

### 4.3 串口 `ADC get freq`（自动分析）

```
ADC_dispatch_command
  └─ ADC_ensure_wave_result()                           (USART_task.c)
       ├─ wavAnl_is_valid()  → s_has_result && s_resultGen == ADC_get_generation()
       │     ├─ 为真：直接返回 true
       │     └─ 为假：
       │          ├─ wavAnl_update()   失败 → 返回 false（分析函数已打印原因）
       │          └─ 成功 → ADC_menu_refresh_wave()，返回 true
  └─ 通过 → wavAnl_get_freq()  → 内部再检查一次 wavAnl_is_valid()
```

这里有一个小细节：`wavAnl_get_freq()` 自己又检查了一次有效性。这是防御性写法，保证即使调用方忘了检查，也不会返回过期数据。

### 4.4 导出（菜单或串口触发）

导出分两段：**启动**（一次性，同步）和**输出**（每 1 ms 一行，异步）。

#### 4.4.1 启动：`ADC_wave_export_start`

```
ADC_wave_export_start()                                  (menu_ADC_suppl.c)
  ├─ serial_export_busy()?  是 → 直接返回（静默）
  ├─ ADC_is_window_ready()? 否 → 打印 "window not full"，返回
  ├─ s_exportSnap = MP_alloc(len * 2)     失败 → 打印 [ERR]，返回
  ├─ ADC_snapshot(s_exportSnap, len)      失败 → 打印，ADC_export_release()，返回
  └─ serial_export_start(ADC_export_sample, s_exportSnap, len, ADC_export_release)
       ├─ s_busy 或 src 为空或 count 为 0 → 返回 false
       └─ 保存 src / ctx / count / done，s_idx = 0，s_busy = true → 返回 true
       失败 → ADC_export_release()
```

启动成功后，函数返回，**快照留在 `s_exportSnap` 里**。

#### 4.4.2 输出：每 1 ms 一次

```
scheduler_run()                                          (scheduler.c，每 1 ms)
  └─ serial_export_proc()
       ├─ s_busy 为假 → 返回
       ├─ s_idx < s_count？
       │    是 → USART_printf("%u.: %lu", s_idx, s_src(s_ctx, s_idx))
       │         s_idx++
       │         返回
       └─ 否（全部输出完）
            ├─ s_busy = false
            └─ s_done()  → ADC_export_release()
                             ├─ MP_free(s_exportSnap)
                             └─ s_exportSnap = NULL
```

每次进入 `serial_export_proc`，要么输出一行，要么结束。**一次只做一件事**，这样不会长时间占住主循环。

#### 4.4.3 导出期间会发生什么

* `ADC_wave_export_start` 不会重置窗口，所以**代数不变，分析结果依然有效**。
* 导出读的是快照，DAC 在导出期间改设置，不影响正在输出的数据。下一次导出才会用新数据。
* 再次发送导出指令时，`serial_export_busy()` 为真，直接返回，不回应。这是目前的已知限制。

### 4.5 菜单 `ADCWaveParam`（参数页）

```
菜单确认键 → menu_navigate(ENTER)
  └─ 动作回调 ADC_get_wave_param()                        (menu_ADC_suppl.c)
       ├─ wavAnl_update()        → 分析一次（失败则保留旧结果）
       └─ menu_ADC_param_enter() → s_menu_mode = MENU_MODE_WAVE_PARAM

OLED 绘制（OLED_page.c）
  └─ OLED_wave_param_page_render()
       └─ wavAnl_menu_info(&period, &vpp, &index)
            ├─ wavAnl_is_valid() 为假 → 返回 NULL → 显示 "waiting for sampling..."
            └─ 为真 → 返回类型名，并填出 period、vpp

返回键 → menu_handle_key → menu_return_normal_mode()
```

参数页是**显示快照**，不是实时刷新。页面打开期间，串口发 `ADC analyze` 会通过 `ADC_menu_refresh_wave()` 重绘。

### 4.6 采样倍率 `ADC set ratio`

```
串口 "ADC set ratio double"  或  菜单 double 动作
  └─ ADC_set_ratio(1)                                    (ADC_service.c)
       ├─ isDoubled 已经是 1 → 直接返回（不重置）
       └─ 否则：isDoubled = 1
                ├─ ADC_set_TIM3_ARR()     → ARR 减半
                └─ ADC_reset_window()     → 代数+1
串口路径额外：ADC_menu_refresh_ratio()     → 菜单在 ADCSetSamplFactor 页时重绘
```

### 4.7 硬件侧：采样是怎么发生的

这一段不是软件调用，而是硬件连锁，只要知道结论即可：

```
TIM3 更新事件（TRGO）
  → 触发 ADC1 规则组转换（adc.c 中 ExternalTrigConv = T3_TRGO）
  → DMA（循环模式）把结果写入 s_ADCWaveWindow
  → DMA 写满 1024 个点时 → HAL_ADC_ConvCpltCallback → s_wrapped = true
```

所以"暂停 TIM3"等于暂停采样，这是 `ADC_snapshot` 能用的前提。

---

## 5. 模块之间的依赖关系

```
                        ┌─────────────────────┐
  串口 USART_task.c ───▶│                     │
  菜单 menu_ADC_suppl.c ▶│   ADC_service.c    │◀── DAC_service.c（只调用 DAC_register_change_cb）
                        │                     │
                        └──────────▲──────────┘
                                   │ ADC_get_window_len / ADC_snapshot / ADC_is_window_ready / ADC_get_generation
                                   │
                        ┌──────────┴──────────┐
                        │  wavAnl_service.c   │──▶ waveform_analyzer（分析库，只认回调）
                        └──────────▲──────────┘
                                   │ wavAnl_is_valid / wavAnl_update / wavAnl_get_*
                                   │
  串口 USART_task.c ───────────────┤
  菜单 OLED_page.c ────────────────┘

  menu_ADC_suppl.c ──▶ serial_export_task.c（只认 src / ctx / count / done）
  serial_export_task.c ──▶ USART_task.c（只调用 USART_printf）
```

**依赖方向的规则**：箭头只能指向"更底层、更通用"的模块。

| 模块 | 知道谁 | 不知道谁 |
| :--- | :--- | :--- |
| `DAC_service` | 只有自己 | ADC、菜单、串口导出 |
| `ADC_service` | DAC 的一个注册函数 | DAC 的 getter，wavAnl，导出 |
| `wavAnl_service` | ADC 窗口接口、分析库 | 菜单、串口分发 |
| `serial_export_task` | 只有 USART_printf | ADC、快照的具体类型 |
| `menu_ADC_suppl` | 所有 ADC/DAC/wavAnl/导出的公开函数 | 无（它是应用层，允许知道一切） |

**把"允许知道谁"写清楚很重要**：以后如果发现某个底层模块需要 `#include` 上层的头文件，就是依赖倒置了，要改成回调。

---

## 6. 状态图

### 6.1 ADC 窗口

```
          ┌───────────────────────────────┐
          │                               │
          ▼                               │
   ┌─────────────┐   写够 len 个点      ┌──┴────────┐
   │  NOT READY  │ ───────────────────▶ │   READY   │
   │ 窗口在写入  │  或 DMA 转满过一圈   │ 可以分析  │
   └──────┬──────┘                      └─────┬─────┘
          │                                   │
          └──── ADC_reset_window ◀────────────┘
                （DAC 改设置、ADC 改倍率）
                代数 +1
```

### 6.2 波形分析结果

```
  ┌────────┐  wavAnl_update 成功   ┌────────┐
  │  NONE  │ ────────────────────▶ │ VALID  │  gen = g
  │ 没分析过│                      │ 有效   │
  └────────┘                       └───┬────┘
                                       │ 窗口被重置（代数变成 g+1）
                                       ▼
                                  ┌────────┐  wavAnl_update 成功
                                  │ STALE  │ ───────────────────▶ VALID（gen = g+1）
                                  │ 过期   │
                                  └────────┘
```

**注意**：`wavAnl_update` 失败时，状态不变。如果原来是 `STALE`，它仍然是 `STALE`，之后的 `get` 会再次尝试分析。

### 6.3 导出

```
  ┌──────┐  serial_export_start 成功   ┌───────────┐
  │ IDLE │ ──────────────────────────▶ │ SENDING   │  s_busy = true
  │      │                             │ idx 递增  │
  └──┬───┘                             └─────┬─────┘
     ▲                                       │ idx == count
     │        s_done()（释放快照）             ▼
     └───────────────────────────────── ┌───────────┐
                                        │   DONE    │  s_busy = false
                                        └───────────┘
```

启动失败的路径：直接回到 `IDLE`，快照已在 `ADC_export_release` 里释放。

---

## 7. 常见疑问

**Q1：为什么 `get freq` 不直接返回上一次的结果？**
A：因为 DAC 可能已经改过设置。代数对不上就说明结果来自旧的窗口，返回它就是错的。所以代码先检查有效性，无效时自动重新分析。

**Q2：分析失败后，旧结果还在吗？**
A：`s_info` 和 `s_has_result` 都还在，但只要代数变了，`wavAnl_is_valid()` 就是假，外部看不到旧数据。

**Q3：导出期间能改 DAC 吗？会不会导出的数据前后不一样？**
A：能改。导出的是快照，是某一时刻的完整副本，所以导出的数据内部是一致的。改动只会影响下一次导出。

**Q4：为什么快照要 `MP_alloc`，不能直接用 `s_ADCWaveWindow`？**
A：`s_ADCWaveWindow` 是 DMA 的目标，硬件随时在写它。直接读，会读到新旧混在一起的数据。快照是一份停下来拷贝的副本。

**Q5：为什么导出不需要 freeze（冻结）？**
A：旧版本曾经用"停止 DMA"来冻结窗口，导出失败时容易忘记恢复，造成 ADC 永久卡住。现在用快照代替冻结，失败路径只需要释放内存。

**Q6：`s_ADCWaveWindow` 没有 `static`，这有什么问题？**
A：别的文件也能 `extern` 它，直接读写会绕过 `ADC_snapshot`，破坏数据一致性。目前没有这样做，但它是一个潜在的破口。

---

## 8. 读代码的推荐顺序

如果你想自己跟一遍，建议按这个顺序读：

1. `Components/ADC_service.h`：看它对外暴露什么。这是"不透明"的边界。
2. `Components/ADC_service.c`：重点看 `ADC_on_DAC_change`、`ADC_reset_window`、`ADC_is_window_ready`、`ADC_snapshot`。
3. `Components/wavAnl_service.c`：看 `wavAnl_update` 的四步，以及 `wavAnl_is_valid`。
4. `Components/serial_export_task.h` 和 `.c`：只有六七行，是理解"回调 + ctx"最好的例子。
5. `Components/menu/menu_ADC_suppl.c` 中的 `ADC_export_sample` 和 `ADC_wave_export_start`：看 ctx 怎么被还原，以及快照怎么被释放。
6. `Components/USART_task.c` 中的 `ADC_ensure_wave_result` 和 `ADC_dispatch_command`：看串口怎么用这些函数。

---

## 9. 自己验证的练习

每一题都能只靠读代码回答。答案在括号后面，先自己想，再对照。

1. 当前 DAC 周期是 2000 ms，你发送 `DAC set period 2000`。`s_generation` 会变吗？（不会。值没变，`DAC_set_period` 直接返回，不通知。）
2. 窗口是 single（512 点），你发送 `ADC set ratio single`。`s_generation` 会变吗？（不会。`ADC_set_ratio` 发现倍率没变，直接返回。）
3. 你先 `ADC analyze`（得到结果，gen = 5），再 `DAC set FSR 1000`，再 `DAC set FSR 3300`，最后 `ADC get freq`。会返回旧结果吗？（不会。两次 FSR 变化都重置了窗口，代数变成 7。`get` 发现过期，自动重新分析。）
4. 导出进行到一半，你发送 `ADC analyze`。导出会出错吗？（不会。导出用的是 `s_exportSnap`，分析用的是另一块 `MP_alloc` 的快照，两者互不影响。）
5. 导出进行中，你再次发送 `ADC wave export`。会发生什么？（`serial_export_busy()` 为真，直接返回，没有任何输出。）
6. 把 `ADC_export_release` 从 `done` 参数里去掉会怎样？（导出结束后快照没有释放，每次导出都会泄漏一块 2048 字节的内存，池子很快用完。）
