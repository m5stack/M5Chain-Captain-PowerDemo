# L0 关机

[English](README.md) | [简体中文](README_CN.md)

## 目的

进入 L0 出货关机态，这是固件能达到的最深状态：PMIC 切断 ESP32、IMU 供电域与全部负载开关，只保留常供电的 3V3_L0 域（PMIC、RTC）；IO 扩展芯片随 ESP32 一起断电。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_power`: L0 / L1 / L2 状态切换与唤醒源配置
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| M5PM1 | 关闭充电、升压与电源指示灯并回读确认；GPIO0 / GPIO3 拉低；关闭看门狗；清除 LDO 保持；调用 `shutdown()` |
| M5IOE1 | 全部 L3B 使能拉低、背光与 RGB 关、中断输出禁用；它自身随 3V3_L2 一起断电，且 L3B 使能受 `PM_3V3_L2_EN` 总门控 |
| RX8130 | 不操作（靠 3V3_L0 继续走时） |
| BMI270 | 不初始化（关机后 3V3_L1 消失） |
| 固件日志 | `BOARD_OK PM1=1 IOE=1` |

## 运行方式

完成静默处理后打印 `TEST_READY`，随后等 3 s，留出拔 USB 的时间，再由 PMIC 切断整机。

## 构建变体

| 覆盖文件 | `config_id` | 作用 |
|---|---|---|
| 默认 | `l0-shutdown` | `CHG_EN = 0`（产品配置） |
| `sdkconfig.charge-on.defaults` | `l0-shutdown` | `CHG_EN = 1`，仅诊断用（日志出现 `L0_EXPERIMENT charge_en=1`） |

两个构建只差充电使能这一位；它是关机前最后一次 PMIC 写入。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 9.81 µA
- 4.2 V: 9.66 µA

在充电使能关闭（`CHG_EN = 0`，产品配置）下测得。打开充电使能后 L0 电流约升高 5 µA。

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
| 测试 ID | `l0-shutdown` |
| 配置 ID | `l0-shutdown` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

默认构建就是产品配置（`CHG_EN = 0`）；要做打开充电的诊断构建，请叠加 `sdkconfig.charge-on.defaults`。

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`TEST_START` 中 `reset_reason=power_on` → `BOARD_OK PM1=1 IOE=1` → `TEST_READY` → `STATE_ENTERED mode=l0_shutdown ldo=off` → 串口消失且不再重新枚举（说明整机确实断电）。
