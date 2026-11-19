# 背光档位

[English](README.md) | [简体中文](README_CN.md)

## 目的

在同一帧静态画面下，分别测量背光 0 %、50 %、100 % 三档的整机电流。三个构建的显示流量完全一致，差异只来自背光。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_display`: JD9853 + LVGL 初始化、静态 / 全白画面与持续重绘
- `m5cc_imu`: BMI270 初始化、采样与 any-motion 配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| M5IOE1 | 打开 TFT 电源轨；背光使能拉高并给设定占空比（IOE G11、PWM 通道 3、20 kHz）；其余 L3B 使能全部拉低，RGB 关 |
| M5PM1 | 关闭充电、升压与电源指示灯；Grove / Chain 5 V 关；看门狗关闭 |
| RX8130 | 初始化，清除并禁用闹铃与定时器中断 |
| BMI270 | 初始化后关闭传感器并开启高级省电 |
| 固件日志 | `BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1 BACKLIGHT=<level>` → `STATE_ENTERED mode=backlight_levels brightness=<level> refresh_hz=0` |

## 运行方式

固件绘制与 `display_only` 相同的静态画面，然后应用编译时选择的档位。测量期间画面不变，三个构建之间唯一的变量就是背光占空比。

## 工作模式

| `config_id` | 覆盖文件 | 背光 |
|---|---|---:|
| `backlight-0` | `sdkconfig.backlight-0.defaults` | 0 % |
| `backlight-50` | `sdkconfig.backlight-50.defaults` | 50 % |
| `backlight-100` | `sdkconfig.backlight-100.defaults` | 100 % |

背光档位是写入 IOE PWM 通道 3 的百分比（`CONFIG_M5CC_BACKLIGHT_PERCENT`），这条路径没有 12 位占空比寄存器。

## 对照

100 % 构建与 `display_only` 功能相同；两者都测是一次有意的对照，用来确认两条固件路径结果一致。

## 0 % 不等于断电

0 % 时 TFT 电源轨与面板逻辑仍然供电，只是去掉了背光 LED 电流。本项同时回答背光使能脚是否真的关断了LED 串。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

| 变体 | 3.7 V | 4.2 V |
|---|---|---|
| 0 % | 43.85 mA | 39.51 mA |
| 50 % | 75.42 mA | 70.30 mA |
| 100 % | 99.69 mA | 99.50 mA |

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
| 测试 ID | `backlight-levels` |
| 配置 ID | `backlight-levels`, `backlight-0`, `backlight-50`, `backlight-100` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

默认构建是 50 %。另外两档请叠加 `sdkconfig.backlight-0.defaults` 或 `sdkconfig.backlight-100.defaults`。

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`BOARD_OK ... BACKLIGHT=<level>` → `TEST_READY` → `STATE_ENTERED mode=backlight_levels brightness=<level> refresh_hz=0`；画面保持静止，背光亮度与档位一致。
