# M5Chain-Captain-PowerDemo

[English](README.md) | [简体中文](README_CN.md)

## 概述

本仓库提供一组独立的 ESP-IDF 固件工程，用于测量 M5Stack Chain Captain（C160，ESP32-S3R8）的功耗。每个测试单独构建、单独烧录，上电后自动运行，打印固定格式的日志作为证据，然后只保持一个工作状态，方便从外部测量电流。固件本身不测电流：数值来自外部功耗分析仪，报告引用其 `WINDOW Avg`。

工程基于 ESP-IDF 5.5.5。构建、烧录、串口验证与电流测量都需要人工执行。

> **满载安全门槛：** 除非 `CONFIG_M5CC_FULL_LOAD_SAFETY_ACK=y`，`tests/full_load` 不会启动音频、RGB 与连续刷新。打开前请确认治具限流、散热条件与停止流程；K1 / K2 / K3 任一键可立即停止负载。

Chain Captain 有五个电源状态。`3V3_L0` 常供电，供给 PMIC 与 RTC；`3V3_L1` 经 PMIC LDO 供给 IMU 域；`3V3_L2` 经 PMIC DCDC 供给 ESP32-S3 与 IO 扩展芯片，因此 IO 扩展芯片在 L0/L1 下无供电，L2 起才随 ESP32 一起上电。L3B 各使能另受 `PM_3V3_L2_EN` 总门控，该信号在 L0/L1 为关，所以这些电源轨在进入 L2 之前无法被打开；进入 L2 后由 IO 扩展芯片逐路控制屏幕、音频、红外、升压、Header 以及 Grove/Chain 各电源轨。

## 功耗测试结果

功耗以整机平均输入电流表示。除链接的报告另有说明外，每个数值都是约 60 s 测量窗口的 `WINDOW Avg`。

表中只写简要状态，完整配置见详情链接。

| 测试场景 | 设备 / 工作状态 | 3.7 V 平均电流 | 4.2 V 平均电流 | 详情 |
|---|---|---:|---:|---|
| L0 关机 | M5PM1：L0 关机；充电、升压、指示灯全关<br>RX8130：3V3_L0 供电，未配置<br>ESP32-S3 / M5IOE1 / BMI270：断电 | 9.81 µA | 9.66 µA | [详情](tests/l0_shutdown/README_CN.md) |
| L1 待机 | M5PM1：L1 待机；LDO 保持置位<br>RX8130：供电，中断禁用<br>BMI270：挂起并开启高级省电<br>ESP32-S3 / M5IOE1：断电 | 20.87 µA | 20.80 µA | [详情](tests/l1_standby/README_CN.md) |
| L2 深睡 | ESP32-S3：深睡，无唤醒源<br>M5PM1 / M5IOE1：L2 静默状态，空闲休眠 5 s<br>RX8130：3V3_L0 供电，未配置<br>BMI270：上电默认即挂起并开启高级省电（3V3_L1 保持供电） | 94.36 µA | 91.43 µA | [详情](tests/l2_deep_sleep/README_CN.md) |
| IMU 唤醒 L1（50 Hz） | M5PM1：L1 待机，配置 G4 唤醒<br>BMI270：any-motion，50 Hz，INT1 低有效<br>RX8130：未配置（3V3_L0 供电） | 37.48 µA | 37.27 µA | [详情](tests/imu_wake_l1/README_CN.md) |
| IMU 唤醒 L1（0.78 Hz） | M5PM1：L1 待机，配置 G4 唤醒<br>BMI270：any-motion，0.78 Hz，INT1 低有效<br>RX8130：未配置（3V3_L0 供电） | 21.02 µA | 20.54 µA | [详情](tests/imu_wake_l1/README_CN.md) |
| IMU 唤醒 L2（50 Hz） | ESP32-S3：深睡，GPIO14 `ext0`<br>M5PM1：配置 G4 唤醒，空闲休眠 1 s<br>RX8130：未配置<br>BMI270：any-motion，50 Hz，INT1 低有效 | 111.00 µA | 108.55 µA | [详情](tests/imu_wake_l2/README_CN.md) |
| IMU 唤醒 L2（0.78 Hz） | ESP32-S3：深睡，GPIO14 `ext0`<br>M5PM1：配置 G4 唤醒，空闲休眠 1 s<br>RX8130：未配置<br>BMI270：any-motion，0.78 Hz，INT1 低有效 | 94.48 µA | 92.49 µA | [详情](tests/imu_wake_l2/README_CN.md) |
| RTC 闹铃唤醒 L1 | M5PM1：L1 待机，配置 G2 唤醒<br>RX8130：配置闹铃，120 s<br>BMI270：上电默认挂起（未配置） | 21.35 µA | 21.12 µA | [详情](tests/rtc_alarm_wake_l1/README_CN.md) |
| RTC 定时唤醒 L1 | M5PM1：L1 待机，配置 G2 唤醒<br>RX8130：配置 120 s 倒计时定时器<br>BMI270：上电默认挂起（未配置） | 21.15 µA | 20.63 µA | [详情](tests/rtc_time_wake_l1/README_CN.md) |
| RTC 闹铃唤醒 L2 | ESP32-S3：深睡，GPIO14 `ext0`<br>M5PM1：配置 G2 唤醒，**关闭空闲休眠**（保持清醒）<br>RX8130：配置闹铃，120 s<br>BMI270：上电默认挂起（未配置） | 663.63 µA | 663.87 µA | [详情](tests/rtc_alarm_wake_l2/README_CN.md) |
| RTC 定时唤醒 L2 | ESP32-S3：深睡，GPIO14 `ext0`<br>M5PM1：配置 G2 唤醒，**关闭空闲休眠**（保持清醒）<br>RX8130：配置 120 s 倒计时定时器<br>BMI270：上电默认挂起（未配置） | 665.16 µA | 665.77 µA | [详情](tests/rtc_time_wake_l2/README_CN.md) |
| 按键唤醒 L2 | ESP32-S3：深睡，K1 / K2 / K3 EXT1<br>M5PM1：未配置唤醒，空闲休眠 1 s<br>M5IOE1：静默状态，空闲休眠开 | 94.21 µA | 91.40 µA | [详情](tests/key_wake_l2/README_CN.md) |
| L3A 空闲 | ESP32-S3：运行状态，240 MHz，关闭 DFS<br>M5PM1：DCDC 开启<br>M5IOE1：L3B 全关 | 35.85 mA | 32.31 mA | [详情](tests/l3a_idle/README_CN.md) |
| 仅显示 | ESP32-S3：运行状态<br>屏幕：一帧静态画面，背光 100 %<br>其余电源轨全关 | 99.67 mA | 99.36 mA | [详情](tests/display_only/README_CN.md) |
| 背光 0 % | ESP32-S3：运行状态<br>屏幕：同一帧静态画面<br>背光：0 %（LED 关，面板仍供电） | 43.85 mA | 39.51 mA | [详情](tests/backlight_levels/README_CN.md) |
| 背光 50 % | ESP32-S3：运行状态<br>屏幕：同一帧静态画面<br>背光：50 % | 75.42 mA | 70.30 mA | [详情](tests/backlight_levels/README_CN.md) |
| 背光 100 % | ESP32-S3：运行状态<br>屏幕：同一帧静态画面<br>背光：100 % | 99.69 mA | 99.50 mA | [详情](tests/backlight_levels/README_CN.md) |
| 正常工作 | ESP32-S3：运行状态<br>屏幕：背光 50 %，1 Hz 整屏刷新<br>其余电源轨全关 | 75.12 mA | 70.76 mA | [详情](tests/normal_active/README_CN.md) |
| IMU 采样 | ESP32-S3：运行状态<br>BMI270：加速度计 + 陀螺仪 100 Hz，每 10 ms 读取<br>屏幕关闭 | 37.54 mA | 33.75 mA | [详情](tests/imu_sampling/README_CN.md) |
| 红外发射 | ESP32-S3：运行状态<br>红外 LED：38 kHz 载波，以 10 Hz 发送 NEC 帧<br>其余 L3B 全关 | 39.15 mA | 34.96 mA | [详情](tests/ir_tx/README_CN.md) |
| 满载 | ESP32-S3：运行状态<br>屏幕：全白，背光 100 %，30 Hz 重绘<br>音频：1 kHz 音调，最大音量<br>RGB 点亮，IMU 100 Hz | 458.70 mA | 431.60 mA | [详情](tests/full_load/README_CN.md) |
| Wi-Fi 连接 | ESP32-S3：运行状态<br>Wi-Fi：已连接至配置 Wi-Fi，`WIFI_PS_NONE`<br>屏幕关闭 | 109.64 mA | 99.78 mA | [详情](tests/wifi_connected/README_CN.md) |
| Wi-Fi light sleep | ESP32-S3：自动 light sleep<br>Wi-Fi：已连接至配置 Wi-Fi，`WIFI_PS_MAX_MODEM`，监听间隔 3 | 6.19 mA | 6.01 mA | [详情](tests/wifi_light_sleep/README_CN.md) |
| Wi-Fi 周期流量 | ESP32-S3：运行状态<br>Wi-Fi：已连接至配置 Wi-Fi，`WIFI_PS_NONE`<br>ICMP：每 1000 ms 发送 64 字节 | 109.05 mA | 99.94 mA | [详情](tests/wifi_periodic_traffic/README_CN.md) |

所有数值都是在充电使能关闭（`CHG_EN = 0`）时测得；打开充电使能后 L0 电流约升高 5 µA。

## 测量曲线

全部 3.7 V 与 4.2 V 的测量曲线都收录在[功耗测量曲线](docs/power-graphs/README_CN.md)索引中，每个测试项一个目录。

## 仓库结构

```text
components/          内置组件快照（BSP、PMIC、IO 扩展、RTC、BMI270、LVGL）
modules/             共享固件模块（board、power、display、audio、imu、rtc、wifi、test support）
tests/<name>/        每个测试项一个独立工程
docs/power-graphs/   测量截图，每个测试项一个目录
```

## 构建与烧录

需要 ESP-IDF 5.5.5。在测试自己的目录内构建并烧录：

```powershell
cd tests\l3a_idle
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

可选变体通过删除 `sdkconfig` 后把 `SDKCONFIG_DEFAULTS` 设为基础文件加一个覆盖文件来应用，例如：

```powershell
Remove-Item -Force sdkconfig -ErrorAction SilentlyContinue
$env:SDKCONFIG_DEFAULTS = "sdkconfig.defaults;sdkconfig.safety-ack.defaults"
idf.py reconfigure
```

Wi-Fi 三项测试的 SSID、密码与目标 IPv4 默认为空，需要运行 `idf.py menuconfig`，在 `M5CC Power Test` 菜单中自行填写（`CONFIG_M5CC_WIFI_SSID` / `CONFIG_M5CC_WIFI_PASSWORD` / `CONFIG_M5CC_WIFI_TARGET_IPV4`）。USB Type-C 只用于烧录与串口验证。

## 测量方法

1. 连上 USB Type-C，烧录固件，并在串口确认 `TEST_START`、`BOARD_OK` 与 `TEST_READY`。
2. 保持同一固件与配置，拔掉 USB Type-C，只通过测量治具给板子供电。关机与睡眠类测试在状态切换前留 3 s 窗口；工作负载会长期保持状态。
3. 唤醒类测试记录的是“配置完成、等待触发”状态下的平均值；唤醒事件本身用于验证通路，不计入平均。
4. 等电流稳定后，在 3.7 V 与 4.2 V 下各取一次约 60 s 的 `WINDOW Avg`。

`wifi_light_sleep` 在刷出 `STATE_ENTERED` 后会停掉 USB Serial/JTAG 控制台，该状态下端口可能失去响应，这是预期行为。

## 日志约定

`TEST_START`（测试 ID、固件版本、配置哈希、复位原因）、`BOARD_OK` / `BOARD_FAIL`、`TEST_FAIL`、`PROFILE`、`IMU_ARMED`、`RTC_ALARM`、`RTC_TIME`、`TEST_READY`、`STATE_ENTERED`；唤醒类还会打印 `WAKE`、`PMIC_WAKE`、`RTC_WAKE`。工作负载另打印 `LOAD`、`STOP_HINT`、`STATE_EXIT`、`IMU_SAMPLING`；`SAFETY_BLOCK` 表示安全门槛拒绝启动负载。

## 依赖

全部内置于 `components/`：M5PM1 1.0.5、M5IOE1 1.0.7、i2c_bus 1.5.2、cmake_utilities 0.5.3、esp_codec_dev 1.5.1、esp_lvgl_port 2.6.2、LVGL 9.5.0、Chain Captain BSP，以及 RX8130 与 BMI270 驱动。详见 [THIRD_PARTY_NOTICES_CN.md](THIRD_PARTY_NOTICES_CN.md)。

## 许可证

本仓库自有源码采用 [MIT 许可证](LICENSE)。第三方组件仍然适用[第三方组件声明](THIRD_PARTY_NOTICES_CN.md)中的许可证。
