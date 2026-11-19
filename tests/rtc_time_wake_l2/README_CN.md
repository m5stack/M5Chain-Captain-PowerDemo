# RTC 定时唤醒（L2）

[English](README.md) | [简体中文](README_CN.md)

## 目的

用 RX8130 的倒计时定时器把 ESP32-S3 从深睡唤醒：走与闹铃、IMU 相同的 G2 → IRQOUT → GPIO14 通路。刻意让 PMIC 保持清醒，以免漏掉 /INT 的短脉冲。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_power`: L0 / L1 / L2 状态切换与唤醒源配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| RX8130 | 配置 120 s 倒计时定时器并打开定时器中断；不使用日历闹铃 |
| M5PM1 | G2 下降沿唤醒、解除 IRQ 屏蔽，G1 配置为 IRQ 输出，**关闭 I2C 空闲休眠（PMIC 保持清醒）** |
| ESP32-S3 | GPIO14 低电平 `ext0` 唤醒，入睡前清除 PMIC 中断状态 |
| BMI270 | 不初始化；保持上电默认的挂起与高级省电（3V3_L1 持续供电） |
| 固件日志 | `RTC_ALARM armed=+120s mode=timer` → `STATE_ENTERED mode=rtc_time_wake_l2 wake=pmic_g2_to_ext0 io_sleep=0` |

## 运行方式

`STATE_ENTERED` 表示即将进入深睡；120 s 后定时器到期，芯片以 `reset_reason=deep_sleep` 重启。

## 唤醒链路

RX8130 /INT → PMIC G2 → PMG1_IRQOUT 拉低 → ESP32 GPIO14 `ext0` → `wake_cause=ext0`。

## PMIC 保持清醒

RX8130 /INT 的脉冲短于 PMIC 的 I2C 空闲休眠扫描周期，会被漏掉，因此本测试调用 `finalize_l2(false)`：IO 扩展可以空闲休眠，PMIC 必须保持清醒。这就是读数在几百 µA、而不是 ~94 µA 的 L2 基线的原因。

## 测量窗口

定时器每 120 s 唤醒芯片一次，随后芯片重新配置并再次入睡。窗口内只要包含一次唤醒，读数就是该循环的平均值（约 665 µA），而不是纯静态电流。建议在 `STATE_ENTERED` 之后 100 s 内取窗口，或临时把定时器周期改长。

## 一次性定时器

倒计时定时器只触发一次。错过唤醒窗口时复位重新配置即可，不需要重新烧录。

## 独立工程

日历闹铃版本是独立工程 `tests/rtc_alarm_wake_l2`，与参考功耗工程的目录结构一致。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 665.16 µA
- 4.2 V: 665.77 µA

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
| 测试 ID | `rtc-time-wake-l2` |
| 配置 ID | `rtc-time-wake-l2` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`RTC_ALARM armed=+120s mode=timer` → `STATE_ENTERED mode=rtc_time_wake_l2 wake=pmic_g2_to_ext0 io_sleep=0` → 串口消失 → 定时器到期后芯片以 `reset_reason=deep_sleep wake_cause=ext0` 重启，并打印 `RTC_WAKE timer=1`。
