# 按键唤醒（L2）

[English](README.md) | [简体中文](README_CN.md)

## 目的

以三个用户按键作为唯一唤醒源，测量深睡等待期间的整机电流。按键直连 ESP32-S3，不经过 PMIC，因此这条唤醒路径只在 L2 成立。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_imu`: BMI270 初始化、采样与 any-motion 配置
- `m5cc_power`: L0 / L1 / L2 状态切换与唤醒源配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| ESP32-S3 | GPIO1 / GPIO4 / GPIO5（K1 / K2 / K3）EXT1 唤醒，任意一个拉低即唤醒，使能内部上拉；隔离 PMIC 中断脚 |
| M5PM1 | 安全态：不配置任何 PMIC 唤醒，关闭充电 / 升压 / 指示灯，看门狗关闭，设置 LDO 保持，I2C 空闲休眠 1 s |
| M5IOE1 | 全部 L3B 使能拉低、背光与 RGB 关、空闲休眠打开 |
| RX8130 | 初始化，清除并禁用闹铃与定时器中断 |
| BMI270 | 初始化后关闭传感器并开启高级省电 |
| 固件日志 | `STATE_ENTERED mode=key_wake_l2 wake=ext1_k1k2k3 io_sleep=1` → `WAKE cause=ext1 ext1_mask=0x...` |

## 运行方式

`TEST_READY` 之后等 3 s，先等按键松开（最多 10 s，避免唤醒时仍按住导致反复唤醒），再配置 EXT1 并进入深睡。按下任意按键整机重新启动，下一次启动会先打印 EXT1 掩码再重新配置。

## 唤醒链路

K1 / K2 / K3 → ESP32-S3 GPIO1 / 4 / 5 EXT1（任意低电平）→ 唤醒。全程不经过 PMIC，因此本构建的 `PMIC_WAKE` 必须保持为 0。

## 为什么不做 L1

按键只连到 ESP32，PMIC 关机态下无法拉起整机，所以本板不存在按键唤醒的 L1 版本。

## 唤醒证据

`WAKE cause=ext1 ext1_mask=0x...`：bit1 是 K1（GPIO1）、bit4 是 K2（GPIO4）、bit5 是 K3（GPIO5）；0x32 表示三个键同时被按下。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 94.21 µA
- 4.2 V: 91.40 µA

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
| 测试 ID | `key-wake-l2` |
| 配置 ID | `key-wake-l2` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=key_wake_l2 wake=ext1_k1k2k3 io_sleep=1` → 串口消失 → 按下 K1、K2 或 K3 后整机重启并打印 `WAKE cause=ext1 ext1_mask=0x...`。
