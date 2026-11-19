# L1 待机

[English](README.md) | [简体中文](README_CN.md)

## 目的

关机但同时刻意保留 3V3_L1（IMU 域），这正是运动唤醒固件等待触发时所处的状态。本项不配置任何唤醒源，只能按电源键恢复。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_imu`: BMI270 初始化、采样与 any-motion 配置
- `m5cc_power`: L0 / L1 / L2 状态切换与唤醒源配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| M5PM1 | 关闭充电、升压与指示灯；**设置** LDO 保持；GPIO0 / GPIO3 拉低；关闭看门狗；`shutdown()` |
| M5IOE1 | 全部 L3B 使能拉低、中断输出禁用；关机时随 3V3_L2 一起断电，使能受 `PM_3V3_L2_EN` 总门控 |
| RX8130 | 初始化，清除并禁用闹铃与定时器中断 |
| BMI270 | 初始化后关闭传感器并开启高级省电 |
| 固件日志 | `BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` |

## 运行方式

`TEST_READY` 之后等 3 s，打印 `STATE_ENTERED`，设置 LDO 保持并调用 `shutdown()`；ESP32 与所有负载开关断电，3V3_L1 继续供电。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 20.87 µA
- 4.2 V: 20.80 µA

与仓库其它结果一致，本项也是在充电使能关闭的状态下测量。打开充电使能后电流会高约 5 µA。

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
| 测试 ID | `l1-standby` |
| 配置 ID | `l1-standby` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=l1_standby wake=none ldo_hold=1` → 串口消失且不再重新枚举。
