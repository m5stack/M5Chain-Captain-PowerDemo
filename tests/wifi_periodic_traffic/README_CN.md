# Wi-Fi 周期流量

[English](README.md) | [简体中文](README_CN.md)

## 目的

测量已连接 STA 每秒发送一个 64 字节 ICMP 回显请求时的整机电流，代表设备与服务器保持轻量心跳的场景。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态
- `m5cc_wifi`: STA 连接、省电模式与周期 ICMP

## 模块配置

| 模块 | 配置 |
|---|---|
| ESP32-S3 | STA 已连接，`WIFI_PS_NONE`，每 1000 ms 向配置的 IPv4 目标发送一个 64 字节 ICMP 回显请求 |
| M5IOE1 | 全部 L3B 使能拉低、背光与 RGB 关、空闲休眠打开 |
| M5PM1 | 安全态：关闭充电 / 升压 / 指示灯，看门狗关闭，设置 LDO 保持，不配置任何唤醒 |
| RX8130 | 初始化，清除并禁用闹铃与定时器中断 |
| BMI270 | 保持上电默认（挂起并开启高级省电），不配置传感器 |
| 固件日志 | `BOARD_OK ... WIFI=connected PS=none PING_MS=1000 PAYLOAD=64` → `STATE_ENTERED mode=wifi_periodic_traffic ps=none ping_1hz=64B display=0` |

## 运行方式

`TEST_READY` 之后等 3 s 启动流量任务，每秒发送一次 ping 并持续保持；只有在回显根本没发出去时串口才会打印跳过提示。

## 目标地址

`CONFIG_M5CC_WIFI_TARGET_IPV4` 必须是数字 IPv4 地址；固件不做域名解析，避免测量窗口里混入 DNS 流量。

## 负载长度

ping 负载固定为 64 字节（由 Wi-Fi 模块定义），与方案 5.3 一致。

## 凭据

SSID、密码与目标 IPv4 默认为空，需要运行 `idf.py menuconfig`，在 `M5CC Power Test` 菜单中填写。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 109.05 mA
- 4.2 V: 99.94 mA

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
| 测试 ID | `wifi-periodic-traffic` |
| 配置 ID | `wifi-periodic-traffic` |

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

`BOARD_OK ... WIFI=connected PS=none PING_MS=1000 PAYLOAD=64` → `TEST_READY` → `STATE_ENTERED mode=wifi_periodic_traffic ps=none ping_1hz=64B display=0`。
