# IMU 运动唤醒（L2）

[English](README.md) | [简体中文](README_CN.md)

## 目的

用运动把 ESP32-S3 从深睡唤醒：BMI270 把 INT1 拉低，PMIC 经 IRQOUT 送到 GPIO14，由 `ext0` 唤醒源重启芯片。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_imu`: BMI270 初始化、采样与 any-motion 配置
- `m5cc_power`: L0 / L1 / L2 状态切换与唤醒源配置
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| BMI270 | Any-motion 检测，INT1 低有效、非锁存，时长约 160 ms、阈值约 50 mg |
| M5PM1 | G4 下降沿唤醒、解除 IRQ 屏蔽，G1 配置为 IRQ 输出，I2C 空闲休眠 1 s |
| ESP32-S3 | GPIO14 低电平 `ext0` 唤醒，入睡前清除 PMIC 中断状态 |
| RX8130 | 不操作；3V3_L0 供电，中断保持复位默认（禁用） |
| 固件日志 | `STATE_ENTERED mode=imu_wake_l2 wake=pmic_g4_to_ext0 io_sleep=1` |

## 运行方式

`STATE_ENTERED` 表示芯片即将入睡。运动发生后芯片以 `reset_reason=deep_sleep` 重启。

## 唤醒链路

BMI270 INT1（低有效）→ PMIC G4 → PMG1_IRQOUT 拉低 → ESP32 GPIO14 `ext0` → `wake_cause=ext0`。

## 构建变体

| 覆盖文件 | `config_id` | 配置 | 加速度计 ODR |
|---|---|---|---|
| 默认 | `imu-wake-l2` | `low_power` | 50 Hz |
| `sdkconfig.odr-lowest.defaults` | `imu-wake-l2` | `low_power` | 0.78 Hz |
| `sdkconfig.profile-reference.defaults` | `imu-wake-l2-reference` | `reference` | 100 Hz + 陀螺仪 |

## 实测唤醒行为

已实机验证：静止 40 s 完全没有误唤醒；晃动设备后出现 `reset_reason=deep_sleep` 与 `wake_cause=ext0`。

## 日志抓取注意事项

必须用不拉 RTS / DTR 的工具打开端口。`.NET SerialPort` 打开时会断言这两根线，导致芯片复位、`wake_cause` 被改写成 `usb`。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

| 变体 | 3.7 V | 4.2 V |
|---|---|---|
| 50 Hz | 111.00 µA | 108.55 µA |
| 0.78 Hz | 94.48 µA | 92.49 µA |

0.78 Hz 构建仍是 `low_power` 配置，只降低加速度计 ODR；该档位下 IMU 的增量已降到 L2 基线的噪声水平。

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
| 测试 ID | `imu-wake-l2` |
| 配置 ID | `imu-wake-l2`, `imu-wake-l2-reference` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

默认构建是 50 Hz 的 `low_power`；要测 0.78 Hz 请叠加 `sdkconfig.odr-lowest.defaults`，要测 `reference` 请叠加 `sdkconfig.profile-reference.defaults`。两个文件可用分号同时叠加。

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`PROFILE profile=low_power accel=50 Hz ...` → `BOARD_OK PM1=1 IOE=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=imu_wake_l2 wake=pmic_g4_to_ext0 io_sleep=1` → 晃动后 `TEST_START` 出现 `reset_reason=deep_sleep wake_cause=ext0`。
