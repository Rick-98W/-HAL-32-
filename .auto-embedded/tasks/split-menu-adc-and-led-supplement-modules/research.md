# 研究发现

| 关键词 | 来源 | 摘要 | 可信度 | 状态 |
|---|---|---|---|---|
| Menu tree | Components/menu_item.c | Main, LED, ADC, System and Log nodes were constructed in one module. | Source | Confirmed |
| LED actions | Components/LED_task.h | Save/restore, blackout and phase toggle APIs can be reused unchanged. | Source | Confirmed |
| ADC scaffold | Components/menu_item.c, knob_task.c | Value helpers are unused; declared edit APIs have no implementations or callers. | Source | Preserved |
| Build | MDK-ARM/template.uvprojx | Target template uses ARMCLANG, GD32F470VE and the existing Components include path. | Project XML | Confirmed |
| Hardware | Components/menu_* | Refactor changes no pin, DMA, IRQ or timer allocations. | Diff | Confirmed |
