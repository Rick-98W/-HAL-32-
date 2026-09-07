# 自动探测结果（草案，待人工确认）

> 由 `aemb init` 扫描工程文件名/扩展名推断，**可能误判**。
> 确认无误后，请把相关信息手工并入 `index.md` 与 `hw-lock.yaml`，再删除本文件。

- 疑似芯片: STM32F4XX, STM32F429XX, STM32F429
- 疑似框架: STM32CubeMX
- 构建系统: 固件产物(.hex), Keil MDK

## 证据
- STM32F4XX ← stm32f4xx_hal_conf.h
- STM32F429XX ← stm32f429xx.h
- STM32F429 ← template_STM32F429ZGTx.dbgconf
- 固件产物(.hex) ← template.hex
- Keil MDK ← template.uvprojx
- STM32CubeMX ← template.ioc
