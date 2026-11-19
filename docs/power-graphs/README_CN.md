# 功耗测量曲线

[English](README.md) | [简体中文](README_CN.md)

每个测试项一个目录。下列每张图都是引用 `WINDOW Avg` 的原始截图。

两张 RTC 唤醒 L2 截图覆盖一个 120 s 重新配置周期，因此平台段包含唤醒峰值，应按该循环的平均值理解。

## 低功耗状态

### L0 关机

3.7 V

<img src="l0_shutdown/3.7v.png" alt="L0 关机 3.7 V" width="60%">

4.2 V

<img src="l0_shutdown/4.2v.png" alt="L0 关机 4.2 V" width="60%">

### L1 待机

3.7 V

<img src="l1_standby/3.7v.png" alt="L1 待机 3.7 V" width="60%">

4.2 V

<img src="l1_standby/4.2v.png" alt="L1 待机 4.2 V" width="60%">

### L2 深睡

3.7 V

<img src="l2_deep_sleep/3.7v.png" alt="L2 深睡 3.7 V" width="60%">

4.2 V

<img src="l2_deep_sleep/4.2v.png" alt="L2 深睡 4.2 V" width="60%">

## 唤醒源

### IMU 唤醒（L1）

3.7 V / 50 Hz

<img src="imu_wake_l1/3.7v.png" alt="IMU 唤醒（L1） 3.7 V / 50 Hz" width="60%">

4.2 V / 50 Hz

<img src="imu_wake_l1/4.2v.png" alt="IMU 唤醒（L1） 4.2 V / 50 Hz" width="60%">

3.7 V / 0.78 Hz

<img src="imu_wake_l1/3.7v-0.78.png" alt="IMU 唤醒（L1） 3.7 V / 0.78 Hz" width="60%">

4.2 V / 0.78 Hz

<img src="imu_wake_l1/4.2v-0.78.png" alt="IMU 唤醒（L1） 4.2 V / 0.78 Hz" width="60%">

### IMU 唤醒（L2）

3.7 V / 50 Hz

<img src="imu_wake_l2/3.7v.png" alt="IMU 唤醒（L2） 3.7 V / 50 Hz" width="60%">

4.2 V / 50 Hz

<img src="imu_wake_l2/4.2v.png" alt="IMU 唤醒（L2） 4.2 V / 50 Hz" width="60%">

3.7 V / 0.78 Hz

<img src="imu_wake_l2/3.7v-0.78.png" alt="IMU 唤醒（L2） 3.7 V / 0.78 Hz" width="60%">

4.2 V / 0.78 Hz

<img src="imu_wake_l2/4.2v-0.78.png" alt="IMU 唤醒（L2） 4.2 V / 0.78 Hz" width="60%">

### RTC 闹铃唤醒（L1）

3.7 V

<img src="rtc_alarm_wake_l1/3.7v.png" alt="RTC 闹铃唤醒（L1） 3.7 V" width="60%">

4.2 V

<img src="rtc_alarm_wake_l1/4.2v.png" alt="RTC 闹铃唤醒（L1） 4.2 V" width="60%">

### RTC 定时唤醒（L1）

3.7 V

<img src="rtc_time_wake_l1/3.7v.png" alt="RTC 定时唤醒（L1） 3.7 V" width="60%">

4.2 V

<img src="rtc_time_wake_l1/4.2v.png" alt="RTC 定时唤醒（L1） 4.2 V" width="60%">

### RTC 闹铃唤醒（L2）

3.7 V

<img src="rtc_alarm_wake_l2/3.7v.png" alt="RTC 闹铃唤醒（L2） 3.7 V" width="60%">

4.2 V

<img src="rtc_alarm_wake_l2/4.2v.png" alt="RTC 闹铃唤醒（L2） 4.2 V" width="60%">

### RTC 定时唤醒（L2）

3.7 V

<img src="rtc_time_wake_l2/3.7v.png" alt="RTC 定时唤醒（L2） 3.7 V" width="60%">

4.2 V

<img src="rtc_time_wake_l2/4.2v.png" alt="RTC 定时唤醒（L2） 4.2 V" width="60%">

### 按键唤醒（L2）

3.7 V

<img src="key_wake_l2/3.7v.png" alt="按键唤醒（L2） 3.7 V" width="60%">

4.2 V

<img src="key_wake_l2/4.2v.png" alt="按键唤醒（L2） 4.2 V" width="60%">

## 工作负载

### L3A 空闲

3.7 V

<img src="l3a_idle/3.7v.png" alt="L3A 空闲 3.7 V" width="60%">

4.2 V

<img src="l3a_idle/4.2v.png" alt="L3A 空闲 4.2 V" width="60%">

### 仅显示

3.7 V

<img src="display_only/3.7v.png" alt="仅显示 3.7 V" width="60%">

4.2 V

<img src="display_only/4.2v.png" alt="仅显示 4.2 V" width="60%">

### 背光档位

3.7 V / 0 %

<img src="backlight_levels/3.7v-0.png" alt="背光档位 3.7 V / 0 %" width="60%">

4.2 V / 0 %

<img src="backlight_levels/4.2v-0.png" alt="背光档位 4.2 V / 0 %" width="60%">

3.7 V / 50 %

<img src="backlight_levels/3.7v-50.png" alt="背光档位 3.7 V / 50 %" width="60%">

4.2 V / 50 %

<img src="backlight_levels/4.2v-50.png" alt="背光档位 4.2 V / 50 %" width="60%">

3.7 V / 100 %

<img src="backlight_levels/3.7v-100.png" alt="背光档位 3.7 V / 100 %" width="60%">

4.2 V / 100 %

<img src="backlight_levels/4.2v-100.png" alt="背光档位 4.2 V / 100 %" width="60%">

### 正常工作

3.7 V

<img src="normal_active/3.7v.png" alt="正常工作 3.7 V" width="60%">

4.2 V

<img src="normal_active/4.2v.png" alt="正常工作 4.2 V" width="60%">

### IMU 采样

3.7 V

<img src="imu_sampling/3.7v.png" alt="IMU 采样 3.7 V" width="60%">

4.2 V

<img src="imu_sampling/4.2v.png" alt="IMU 采样 4.2 V" width="60%">

### 红外发射

3.7 V

<img src="ir_tx/3.7v.png" alt="红外发射 3.7 V" width="60%">

4.2 V

<img src="ir_tx/4.2v.png" alt="红外发射 4.2 V" width="60%">

### 满载

3.7 V

<img src="full_load/3.7v.png" alt="满载 3.7 V" width="60%">

4.2 V

<img src="full_load/4.2v.png" alt="满载 4.2 V" width="60%">

## Wi-Fi 负载

### Wi-Fi 连接

3.7 V

<img src="wifi_connected/3.7v.png" alt="Wi-Fi 连接 3.7 V" width="60%">

4.2 V

<img src="wifi_connected/4.2v.png" alt="Wi-Fi 连接 4.2 V" width="60%">

### Wi-Fi light sleep

3.7 V

<img src="wifi_light_sleep/3.7v.png" alt="Wi-Fi light sleep 3.7 V" width="60%">

4.2 V

<img src="wifi_light_sleep/4.2v.png" alt="Wi-Fi light sleep 4.2 V" width="60%">

### Wi-Fi 周期流量

3.7 V

<img src="wifi_periodic_traffic/3.7v.png" alt="Wi-Fi 周期流量 3.7 V" width="60%">

4.2 V

<img src="wifi_periodic_traffic/4.2v.png" alt="Wi-Fi 周期流量 4.2 V" width="60%">

## 命名

```text
<voltage>v.png                  default capture
<voltage>v-0.78.png             IMU wake, 0.78 Hz profile
<voltage>v-0.png                backlight 0 %
<voltage>v-50.png               backlight 50 %
<voltage>v-100.png              backlight 100 %
```

## 约定

- 每个测试项、每个电压、每种变体各一张图。
- 报告中引用的数值必须是同一张图的 `WINDOW Avg`。
- 只为保留图例而裁剪，不要缩放或修饰绘图区域。
