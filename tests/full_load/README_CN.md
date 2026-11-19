# 满载

[English](README.md) | [简体中文](README_CN.md)

## 目的

本测试施加固件能制造的最坏负载：100 % 亮度全白屏、30 Hz 整屏刷新、1 kHz 最大音量音调、RGB 点亮、IMU 100 Hz 采样。

## 启用的模块

- `m5cc_audio`: ES8311 Codec 与 AW8737A PA 初始化、音调输出
- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_display`: JD9853 + LVGL 初始化、静态 / 全白画面与持续重绘
- `m5cc_imu`: BMI270 初始化、采样与 any-motion 配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| 显示 | 背光 100 %，全白画面（`show_solid_white`）以 30 Hz 重绘 |
| 音频 | ES8311 + AW8737A（PA Mode 4），MONO 22050 Hz、音量 100，1 kHz 正弦约 −2 dBFS，由独立任务持续输出 |
| RGB | IOE PWM 通道 4 / 2 / 1 分别设为 70 % / 100 % / 100 %（红 / 绿 / 蓝） |
| BMI270 | 加速度计与陀螺仪 100 Hz 采样 |
| 按键 | 轮询 K1 / K2 / K3；任意按键停止音频并把背光降到 50 % |
| 固件日志 | `LOAD lcd=1 audio=1 rgb=1 imu=1 ...` → `STATE_ENTERED mode=full_load` → `STOP_HINT press_any_key=stop max_load=<n>s` |

## 运行方式

负载在 `TEST_READY` 之后启动，可由 K1 / K2 / K3 任一键或自动超时（`CONFIG_M5CC_FULL_LOAD_MAX_SECONDS`，默认 600 s）停止。停止时打印 `STATE_EXIT reason=key` 或 `reason=timeout`，并把背光保持在 50 %。

## 安全门槛

除非 `CONFIG_M5CC_FULL_LOAD_SAFETY_ACK=y`，否则音频、RGB 与连续刷新都不会启动。门槛关闭时固件打印 `SAFETY_BLOCK reason=full_load_not_acked` 并停在 L3A 空闲态。只有在确认治具限流与散热条件后才应打开。

## 构建变体

| 覆盖文件 | `config_id` | 作用 |
|---|---|---|
| 默认 | `full-load` | 门槛关闭，打印 `SAFETY_BLOCK` |
| `sdkconfig.safety-ack.defaults` | `full-load` | 门槛打开，运行满载 |

## 全白画面

屏幕显示纯白画面而不是文字画面，让 LCD 面板与背光同时处于最大负载；随后 30 Hz 整屏重绘再叠加运行时的 SPI 流量。

## 停止流程

按 K1、K2 或 K3 立即停止负载。超时只是无人值守时的兜底；安全门槛与这两者相互独立。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 458.70 mA
- 4.2 V: 431.60 mA

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
| 测试 ID | `full-load` |
| 配置 ID | `full-load` |

## 构建与烧录

本测试在默认配置下无法运行；先删除 `sdkconfig`，再叠加所需覆盖文件并重新配置：

```powershell
Remove-Item -Force sdkconfig -ErrorAction SilentlyContinue
$env:SDKCONFIG_DEFAULTS = "sdkconfig.defaults;sdkconfig.safety-ack.defaults"
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

默认构建会打印 `SAFETY_BLOCK`；要真正跑满载必须叠加 `sdkconfig.safety-ack.defaults`。

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `LOAD lcd=1 audio=1 rgb=1 imu=1 brightness=100 volume=100 tone_hz=1000 draw_hz=30` → `STATE_ENTERED mode=full_load` → `STOP_HINT press_any_key=stop max_load=600s`。屏幕全白、RGB 亮起、喇叭有明显声音；按任意键后打印 `STATE_EXIT reason=key`。
