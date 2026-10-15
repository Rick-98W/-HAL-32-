# 编辑清单

| 文件 | 改动 | 验证标准 | 结果 | commit |
|---|---|---|---|---|
| Components/menu_item.c | Retain main tree, shared mode and input dispatch; initialize supplement modules. | Preserve navigation | Reviewed | Not committed |
| Components/menu_LED_suppl.c/.h | Own five LED nodes, callbacks, toggle feedback and WKUP exit. | Preserve LED behavior | Compiled | Not committed |
| Components/menu_ADC_suppl.c/.h | Own two DAC nodes and existing unbound value editing scaffold. | Preserve ADC tree | Compiled | Not committed |
| MDK-ARM/template.uvprojx | Add both module pairs to existing component group. | Compile and link | 0 errors, 0 warnings | Not committed |

## Verification
- keil_builder.py --project MDK-ARM/template.uvprojx --uv4 F:/programs/Keil/MDK/UV4/UV4.exe: PASS, template / GD32F470VE / ARMCLANG V6.22, 1 second.
- Size: Code 49532, RO-data 4700, RW-data 296, ZI-data 26784; AXF and HEX generated.
- check.py --hw --spec: ALL PASS. Hardware lock is currently empty; source diff adds no hardware configuration.
- check.py default: architecture runner fails to parse UTF-8 under Windows PowerShell.
- Git Bash arch-check.sh --all: 10 pre-existing violations in Core/Src/main.c and Packages/u8g2; none in changed menu files. Full project architecture gate remains failing.
- Public menu APIs/page IDs and menu topology retained; ADC edit scaffold remains unbound.
- No board execution or flashing performed.
