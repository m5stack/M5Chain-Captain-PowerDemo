# 红外发射

[English](README.md) | [简体中文](README_CN.md)

## 目的

在红外 LED 持续发送 NEC 帧时测量整机电流。本构建中 IR 电源轨是唯一的 L3B 负载，因此与 L3A 空闲的差值即为发射器的成本。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| ESP32-S3 | GPIO48 上的 RMT 发送通道，1 µs 分辨率，38 kHz 载波、33 % 占空比，每 100 ms 发送一帧 NEC |
| M5IOE1 | IR 电源轨（`IOEG2_IR_L3B_EN`）拉高；背光、RGB 与其余 L3B 使能全部拉低 |
| M5PM1 | 安全态：关闭充电 / 升压 / 指示灯，看门狗关闭，设置 LDO 保持，不配置任何唤醒 |
| RX8130 | 初始化，清除并禁用闹铃与定时器中断 |
| BMI270 | 保持上电默认（挂起并开启高级省电），不配置传感器 |
| 固件日志 | `BOARD_OK ... IR=nec carrier_hz=38000 duty=0.33 frames_per_s=10` → `STATE_ENTERED mode=ir_tx nec_38khz_10hz display=0` |

## 运行方式

`TEST_READY` 之后等 3 s，然后每 100 ms 发送一帧 NEC（地址 0x00、命令 0x00），只要不停电就一直持续。

## 帧结构

9 ms 起始标记、4.5 ms 起始间隔、32 位数据（0 为 560 µs / 560 µs，1 为 560 µs / 1690 µs），最后是停止标记。帧间间隔由发送循环产生，不使用超长 RMT 时长。

## 引脚

IR 发送 = GPIO48。本工程不引用红外接收端（GPIO47）。

## 为什么用这个负载

常开 38 kHz 载波会让 LED 一直点亮；每秒 10 帧与真实遥控器的工作方式接近，读数更有代表性。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 39.15 mA
- 4.2 V: 34.96 mA

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
| 测试 ID | `ir-tx` |
| 配置 ID | `ir-tx` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`BOARD_OK ... IR=nec ...` → `TEST_READY` → `STATE_ENTERED mode=ir_tx nec_38khz_10hz display=0`。
