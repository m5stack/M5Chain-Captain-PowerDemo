# Wi-Fi 连接（关闭省电）

[English](README.md) | [简体中文](README_CN.md)

## 目的

测量 STA 连接 AP 且关闭 Wi-Fi 省电时的整机电流，此时射频在信标间隔内不会休眠，是已连接状态的最坏情况。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态
- `m5cc_wifi`: STA 连接、省电模式与周期 ICMP

## 模块配置

| 模块 | 配置 |
|---|---|
| ESP32-S3 | STA 接口，Wi-Fi 省电设为 `WIFI_PS_NONE`，无应用业务，连接后不做扫描 |
| M5IOE1 | 全部 L3B 使能拉低、背光与 RGB 关、空闲休眠打开 |
| M5PM1 | 安全态：关闭充电 / 升压 / 指示灯，看门狗关闭，设置 LDO 保持，不配置任何唤醒 |
| RX8130 | 初始化，清除并禁用闹铃与定时器中断 |
| BMI270 | 保持上电默认（挂起并开启高级省电），不配置传感器 |
| 固件日志 | `BOARD_OK ... WIFI=connected PS=none CHANNEL=<n> RSSI_DBM=<n>` → `STATE_ENTERED mode=wifi_connected ps=none traffic=0 display=0` |

## 运行方式

`TEST_READY` 之后等 3 s 并保持连接状态。测量窗口内串口无输出，因此读数就是连接本身的开销。

## 凭据

SSID、密码与目标 IPv4 默认为空，需要运行 `idf.py menuconfig`，在 `M5CC Power Test` 菜单中填写；日志不会打印这些值。

## 事件任务栈

Wi-Fi 事件回调会打印分配到的 IPv4 地址，因此把 `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` 从默认 2304 提到 4096，避免 `sys_evt` 任务栈溢出。

## 射频状态

`WIFI_PS_NONE` 让射频在信标间隔内也保持唤醒；与 `wifi_light_sleep` 对比即可看出省电带来的收益。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 109.64 mA
- 4.2 V: 99.78 mA

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
| 测试 ID | `wifi-connected` |
| 配置 ID | `wifi-connected` |

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

`BOARD_OK ... WIFI=connected PS=none ...` → `TEST_READY` → `STATE_ENTERED mode=wifi_connected ps=none traffic=0 display=0`。
