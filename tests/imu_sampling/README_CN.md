# IMU 采样

[English](README.md) | [简体中文](README_CN.md)

## 目的

在 BMI270 连续采集加速度计与陀螺仪数据、屏幕 / 音频 / RGB 等负载全部关闭的条件下测量运行状态下的整机电流，用于把 IMU 及其 I2C 流量单独分离出来。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_imu`: BMI270 初始化、采样与 any-motion 配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| BMI270 | 加速度计与陀螺仪 100 Hz、±2 g / ±2000 dps、性能模式、关闭高级省电；后台任务每 10 ms 读一次 |
| M5PM1 | 关闭充电、升压与电源指示灯；Grove / Chain 5 V 关 |
| M5IOE1 | 全部 L3B 使能拉低、背光关、RGB 关、中断输出禁用 |
| RX8130 | 初始化，清除并禁用闹铃与定时器中断 |
| 显示与音频 | 不启动（无背光、无 Codec 电源轨、无 I2S 流量） |
| 固件日志 | `IMU_SAMPLING hz=100 samples=<n>` → `STATE_ENTERED mode=imu_sampling accel_hz=100 gyro_hz=100 display=off` |

## 运行方式

`TEST_READY` 之后固件使能传感器、启动采样任务并打印 `IMU_SAMPLING`；整个测量窗口内任务持续读芯片，主任务只做延时。

## 固定配置

本项只有一种固定配置（加速度计 + 陀螺仪 100 Hz、每 10 ms 采样一次），不提供可选模式。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 37.54 mA
- 4.2 V: 33.75 mA

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
| 测试 ID | `imu-sampling` |
| 配置 ID | `imu-sampling` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `IMU_SAMPLING hz=100 samples=<n>` → `STATE_ENTERED mode=imu_sampling accel_hz=100 gyro_hz=100 display=off`；重新运行时 `<n>` 会继续增长。
