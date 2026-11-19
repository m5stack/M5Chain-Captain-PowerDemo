# 正常工作

[English](README.md) | [简体中文](README_CN.md)

## 目的

贴近实际使用的状态：屏幕点亮、背光 50 %、每秒整屏刷新一次，其余外设全部断电。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_display`: JD9853 + LVGL 初始化、静态 / 全白画面与持续重绘
- `m5cc_imu`: BMI270 初始化、采样与 any-motion 配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| 显示 | 通过工厂 BSP 与 LVGL 9 驱动 JD9853，背光 50 %（IOE PWM 通道 3，20 kHz，占空比 = 50 %），每秒整屏重绘 |
| M5PM1 | 关闭充电、升压与指示灯；设置 LDO 保持；DCDC 保持开启 |
| M5IOE1 | 只打开 LCD 电源轨与背光；音频、RGB、红外、升压、Header、Grove / Chain 均关闭 |
| BMI270 / RX8130 | BMI270 挂起，RTC 中断禁用 |
| 固件日志 | `STATE_ENTERED mode=normal_active brightness=50 refresh_hz=1` |

## 运行方式

显示栈在 `TEST_READY` 之后才启动；第一帧刷新完成后打印 `STATE_ENTERED`，随后以 1 Hz 持续刷新，串口无输出。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 75.12 mA
- 4.2 V: 70.76 mA

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
| 测试 ID | `normal-active` |
| 配置 ID | `normal-active` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=normal_active brightness=50 refresh_hz=1`，屏幕持续刷新。
