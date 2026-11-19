# L2 深睡

[English](README.md) | [简体中文](README_CN.md)

## 目的

让 ESP32-S3 进入深睡：所有负载开关与外设全部关闭，且不配置任何唤醒源，因此只有断电重启才能退出该状态。

## 启用的模块

- `m5cc_board`: BSP 初始化、电源轨 / GPIO / PWM 控制与静默处理
- `m5cc_imu`: BMI270 初始化、采样与 any-motion 配置
- `m5cc_power`: L0 / L1 / L2 状态切换与唤醒源配置
- `m5cc_rtc`: RX8130 走时、日历闹铃与倒计时定时器
- `m5cc_test_support`: 测试标记、日志约定与安全保持状态

## 模块配置

| 模块 | 配置 |
|---|---|
| ESP32-S3 | 不配置任何唤醒源，调用 `esp_deep_sleep_start()`；ext0 / ext1 均关闭 |
| M5PM1 | 关闭指示灯、充电与升压；5V 输出拉低；设置 LDO 保持（3V3_L1 保持供电）；I2C 空闲休眠 5 s；IRQ 输出拉低 |
| M5IOE1 | 停 PWM 块；LCD、音频、红外、背光、复位全部关闭；RGB 三脚拉低；I2C 空闲休眠 5 s |
| RX8130 | 不操作；由 3V3_L0 供电继续走时，中断保持复位默认（禁用） |
| BMI270 | 不初始化；POR 后传感器默认处于挂起并开启高级省电，3V3_L1 保持供电 |
| 固件日志 | `STATE_ENTERED mode=l2_deep_sleep` |

## 运行方式

打印 `TEST_READY` 后执行关断序列：逐路关闭负载开关与外设，相隔 1 s 分别给 IOE 与 PMIC 设置空闲休眠超时，最后进入深睡。`STATE_ENTERED` 标记这一转换。

## RGB 指示灯引脚配置

RGB 三个引脚（IOE G8 / G9 / G10）先停掉 PWM 块，再作为普通推挽输出保持低电平：

```c
ioe->setPwmFrequency(0);
ioe->pinMode(pin, OUTPUT);
ioe->setDriveMode(pin, M5IOE1_DRIVE_PUSHPULL);
ioe->digitalWrite(pin, LOW);
```

PWM 通道即使占空比为 0 仍有残余导通，引脚浮空时受光的灯珠还会漏电，两者都会多耗 150 µA 以上，因此本状态两者都不采用。需要点亮 RGB 的测试会重新打开 PWM 并设置占空比。

## 实测电流

下表数值为测量记录中的 `WINDOW Avg`。

- 3.7 V: 94.36 µA
- 4.2 V: 91.43 µA

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
| 测试 ID | `l2-deep-sleep` |
| 配置 ID | `l2-deep-sleep` |

## 构建与烧录

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

需要 ESP-IDF v5.5.5。每个工程自带 `sdkconfig.defaults`；不要把其它工程的 `sdkconfig` 拷过来覆盖。测量前拔掉 USB Type-C，取窗口期间不要开 monitor。

## 预期验证结果

`TEST_READY` → `STATE_ENTERED mode=l2_deep_sleep` → 串口消失，设备保持关机直到断电重启。
