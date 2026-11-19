# IMU 运动唤醒（L1）

[English](README.md) | [简体中文](README_CN.md)

## 目的

用运动把整机从 L1 待机态唤醒：BMI270 把 INT1 拉低，PMIC 在 G4 上看到下降沿后重新给整机上电。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_imu`: BMI270 初始化、采样与 any-motion 配置
- `m5cc_power`: L0 / L1 / L2 状态切换与唤醒源配置
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| BMI270 | Any-motion 检测，INT1 低有效、非锁存，时长约 160 ms、阈值约 50 mg |
| M5PM1 | G4 下降沿唤醒、解除 IRQ 屏蔽、设置 LDO 保持，随后 `shutdown()` |
| M5IOE1 | 关机前把全部 L3B 使能拉低 |
| RX8130 | 不操作；3V3_L0 供电，中断保持复位默认（禁用） |
| 固件日志 | `PROFILE profile=<low_power or reference> accel=<ODR> gyro=<ODR or off> feature=any_motion` → `STATE_ENTERED mode=imu_wake_l1 wake=pmic_g4 ldo_hold=1` |

## 运行方式

配置好运动检测后等 3 s 关机。之后任何晃动都会让整机重新上电，启动后打印唤醒证据并再次配置。

## 唤醒链路

BMI270 INT1（低有效）→ PMIC G4（下降沿）→ PMIC 给整机上电 → ESP32 以 `reset_reason=power_on` 启动。

## 构建变体

| 覆盖文件 | `config_id` | 配置 | 加速度计 ODR |
|---|---|---|---|
| 默认 | `imu-wake-l1` | `low_power` | 50 Hz |
| `sdkconfig.odr-lowest.defaults` | `imu-wake-l1` | `low_power` | 0.78 Hz |
| `sdkconfig.profile-reference.defaults` | `imu-wake-l1-reference` | `reference` | 100 Hz + 陀螺仪 |

## 实测唤醒行为

已实机验证：关机后晃动设备，板子自行开机（`PMIC_WAKE src=0x20`，即外部唤醒位），持续晃动时循环复现。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

| 变体 | 3.7 V | 4.2 V |
|---|---|---|
| 50 Hz | 37.48 µA | 37.27 µA |
| 0.78 Hz | 21.02 µA | 20.54 µA |

0.78 Hz 构建仍是 `low_power` 配置，只降低加速度计 ODR；any-motion 时长仍为 8 个采样，等效需要约 10 s 的连续运动才能触发。

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
| 测试 ID | `imu-wake-l1` |
| 配置 ID | `imu-wake-l1`, `imu-wake-l1-reference` |

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

`PROFILE profile=low_power accel=50 Hz ...` → `BOARD_OK PM1=1 IOE=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=imu_wake_l1 wake=pmic_g4 ldo_hold=1` → 串口消失（已关机）→ 晃动后板子重新启动，日志出现 `PMIC_WAKE src=0x20` 与 `WAKE cause=undefined`（全新上电没有 ESP32 唤醒原因）。
