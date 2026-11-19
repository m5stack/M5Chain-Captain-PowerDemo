# L3A 空闲

[English](README.md) | [简体中文](README_CN.md)

## 目的

这是有源基线：ESP32-S3 跑在 240 MHz，所有 L3B 负载开关关闭，因此测量值只包含 ESP32 与常供电域。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_power`: L0 / L1 / L2 状态切换与唤醒源配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| ESP32-S3 | 240 MHz、关闭 DFS，进入状态后无任何应用负载 |
| M5PM1 | 关闭充电、升压与指示灯；设置 LDO 保持；DCDC 保持开启 |
| M5IOE1 | 全部 L3B 使能拉低、中断输出禁用 |
| RX8130 | 初始化，清除并禁用中断 |
| BMI270 | 不初始化；保持上电默认的挂起与高级省电 |
| 固件日志 | `STATE_ENTERED mode=l3a_idle cpu_mhz=240 dfs=off` |

## 运行方式

打印 `STATE_ENTERED` 后只阻塞在 `vTaskDelay(portMAX_DELAY)`，没有任何周期性任务唤醒，串口无输出。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 35.85 mA
- 4.2 V: 32.31 mA

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
| 测试 ID | `l3a-idle` |
| 配置 ID | `l3a-idle` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`BOARD_OK PM1=1 IOE=1 RTC=1` → `TEST_READY` → `STATE_ENTERED mode=l3a_idle cpu_mhz=240 dfs=off`，随后无输出。
