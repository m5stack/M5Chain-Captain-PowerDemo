# 仅显示

[English](README.md) | [简体中文](README_CN.md)

## 目的

在 TFT 供电、背光 100 %、只显示一帧静态画面的条件下测量整机电流，其余外设负载全部关闭。用于把屏幕、屏幕电源轨与背光的开销从纯 CPU 基线中分离出来。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_display`: JD9853 + LVGL 初始化、静态 / 全白画面与持续重绘
- `m5cc_imu`: BMI270 初始化、采样与 any-motion 配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| M5IOE1 | 打开 TFT 电源轨，背光使能拉高并给 100 % PWM 占空比；其余 L3B 使能全部拉低，RGB 关，中断输出禁用 |
| M5PM1 | 关闭充电、升压与电源指示灯；Grove / Chain 5 V 关；看门狗关闭 |
| RX8130 | 初始化，清除并禁用闹铃与定时器中断 |
| BMI270 | 初始化后关闭传感器并开启高级省电 |
| 固件日志 | `STATE_ENTERED mode=display_only brightness=100 refresh_hz=0` |

## 运行方式

`TEST_READY` 之后等 3 s，打开 TFT 电源轨、初始化 JD9853 与 LVGL、背光设为 100 % 并绘制一帧静态画面。之后不再启动任何定时器或刷新任务，测量窗口内画面静止、无周期性 SPI 流量。

## 静态画面

画面在 `STATE_ENTERED` 之前只刷新一次。保持画面不变正是本项与 `normal_active`（每秒整屏重绘一次）的区别。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 99.67 mA
- 4.2 V: 99.36 mA

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
| 测试 ID | `display-only` |
| 配置 ID | `display-only` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=display_only brightness=100 refresh_hz=0`；屏幕保持静止画面，串口在线。
