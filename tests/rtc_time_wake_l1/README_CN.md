# RTC 定时唤醒（L1）

[English](README.md) | [简体中文](README_CN.md)

## 目的

用 RX8130 的倒计时定时器把整机从 L1 待机态唤醒：定时器拉低 /INT，PMIC 在 G2 上看到下降沿后给整机上电。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_power`: L0 / L1 / L2 状态切换与唤醒源配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| RX8130 | 配置 120 s 倒计时定时器并打开定时器中断；不使用日历闹铃 |
| M5PM1 | G2 下降沿唤醒、解除 IRQ 屏蔽、设置 LDO 保持，随后 `shutdown()` |
| M5IOE1 | 关机前把全部 L3B 使能拉低 |
| BMI270 | 不初始化；保持上电默认的挂起与高级省电（3V3_L1 持续供电） |
| 固件日志 | `RTC_ALARM armed=+120s mode=timer` → `STATE_ENTERED mode=rtc_time_wake_l1 wake=pmic_g2 timer=120s ldo_hold=1` |

## 运行方式

配置完成后打印 `RTC_ALARM armed=+120s mode=timer`，随后关机；定时器到点后整机自行上电一次并重新配置。

## 唤醒链路

RX8130 /INT（开漏、低有效）→ PMIC G2 下降沿 → 整机上电 → ESP32 以 `reset_reason=power_on` 启动。

## 一次性定时器

与日历闹铃不同，倒计时定时器只触发一次。错过唤醒窗口时复位重新配置即可，不需要重新烧录。

## 独立工程

日历闹铃版本是独立工程 `tests/rtc_alarm_wake_l1`，与参考功耗工程的目录结构一致。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 21.15 µA
- 4.2 V: 20.63 µA

曲线见[功耗测量曲线索引](../../docs/power-graphs/README_CN.md)。

## 配置

| 项 | 值 |
|---|---|
| 目标芯片 | ESP32-S3R8（16 MB Flash，8 MB 八线 PSRAM） |
| Flash | 16 MB |
| PSRAM | 八线，80 MHz |
| CPU | 240 MHz，关闭 DFS |
| 控制台 | USB Serial/JTAG |
| I2C | 100 kHz |
| 总线器件 | M5PM1 0x6E、M5IOE1 0x4F |
| 测试 ID | `rtc-time-wake-l1` |
| 配置 ID | `rtc-time-wake-l1` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`RTC_ALARM armed=+120s mode=timer` → `STATE_ENTERED mode=rtc_time_wake_l1` → 串口消失 → 板子重新启动并打印 `PMIC_WAKE src=0x20` 与 `RTC_WAKE alarm=0 timer=1`。
