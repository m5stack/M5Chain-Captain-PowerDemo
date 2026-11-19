# Wi-Fi 自动 light sleep

[English](README.md) | [简体中文](README_CN.md)

## 目的

测量已连接 STA 在 Wi-Fi `WIFI_PS_MAX_MODEM` 省电配合 ESP-IDF 自动 light sleep 下的整机电流，这是电池供电产品实际出货会用的配置。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态
- `m5cc_wifi`: STA 连接、省电模式与周期 ICMP

## 模块配置

| 模块 | 配置 |
|---|---|
| ESP32-S3 | STA 已连接，`WIFI_PS_MAX_MODEM`，监听间隔取 `CONFIG_M5CC_WIFI_LISTEN_INTERVAL`，开启自动 light sleep 与动态调频（80–240 MHz） |
| M5IOE1 | 全部 L3B 使能拉低、背光与 RGB 关、空闲休眠打开 |
| M5PM1 | 安全态：关闭充电 / 升压 / 指示灯，看门狗关闭，设置 LDO 保持，不配置任何唤醒 |
| RX8130 | 初始化，清除并禁用闹铃与定时器中断 |
| BMI270 | 保持上电默认（挂起并开启高级省电），不配置传感器 |
| 固件日志 | `BOARD_OK ... PS=max_modem LISTEN_INTERVAL=3 CPU_MIN_MHZ=80 CPU_MAX_MHZ=240 LIGHT_SLEEP=1` → `STATE_ENTERED mode=wifi_light_sleep ps=max_modem light_sleep=1 ...` |

## 运行方式

`TEST_READY` 之后等 3 s，打印 `STATE_ENTERED` 并刷空控制台，然后才开启自动 light sleep；此后 CPU 只在需要接收信标时被唤醒。

## light sleep 下的控制台

开启自动 light sleep 后 USB Serial/JTAG 会停摆。`STATE_ENTERED` 会在此之前刷出，因此主机上最后一行就是状态行；端口可能消失或打不开（Windows 上表现为 `PermissionError 13`）。这是预期行为，不是固件故障。请在 `TEST_READY` 之后的 3 s 窗口内看完日志，然后拔掉 USB。

## 电源管理

需要 `CONFIG_PM_ENABLE`、`CONFIG_FREERTOS_USE_TICKLESS_IDLE` 与 `CONFIG_PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP`。固件会回读实际生效的 `esp_pm_config_t`，若 light sleep 标志或频率范围没有生效则判定失败。

## 监听间隔

驱动会报告协商后的监听间隔，必须等于 `CONFIG_M5CC_WIFI_LISTEN_INTERVAL`，否则说明 Modem 的唤醒频率与配置不符。

## 凭据

SSID、密码与目标 IPv4 默认为空，需要运行 `idf.py menuconfig`，在 `M5CC Power Test` 菜单中填写。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 6.19 mA
- 4.2 V: 6.01 mA

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
| 测试 ID | `wifi-light-sleep` |
| 配置 ID | `wifi-light-sleep` |

## 构建与烧录

构建前先填写 Wi-Fi 配置：

```powershell
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

在 `menuconfig` 中打开 `M5CC Power Test` 菜单，填写 SSID、密码与目标 IPv4；默认值为空，未填写前测试无法连接。

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`BOARD_OK ... LIGHT_SLEEP=1 ...` → `TEST_READY` → `STATE_ENTERED mode=wifi_light_sleep ps=max_modem light_sleep=1 traffic=0 display=0`；该行之后控制台停止输出。
