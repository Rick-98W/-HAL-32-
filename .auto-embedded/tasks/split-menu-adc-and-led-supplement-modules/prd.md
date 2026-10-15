# Split menu ADC and LED supplement modules

## 需求 / 验收标准
- Split Components/menu_item.c into menu_ADC_suppl.c/.h and menu_LED_suppl.c/.h.
- Preserve menu names, page IDs, sibling/parent links, LED callbacks and input behavior.
- Register both module pairs in MDK-ARM/template.uvprojx and compile successfully.

## 约束
- L6 application modules follow existing menu_* naming requested by the user.
- Keep existing menu_item.h callers compatible. No peripheral configuration changes.
- ADC value editing is an existing unbound scaffold; activation is outside this refactor.
