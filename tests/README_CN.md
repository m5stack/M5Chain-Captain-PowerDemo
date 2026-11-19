# 功耗测试工程

[English](README.md) | [简体中文](README_CN.md)

每个目录都是一个基于 ESP-IDF 5.5.5 的独立工程，对应 M5Stack Chain Captain 的一种工作状态或负载。目录名就是固件与测量证据使用的测试标识。

| 测试分组 | 目录 |
|---|---|
| 低功耗状态 | `l0_shutdown`, `l1_standby`, `l2_deep_sleep` |
| 唤醒源 | `imu_wake_l1`, `imu_wake_l2`, `rtc_alarm_wake_l1`, `rtc_time_wake_l1`, `rtc_alarm_wake_l2`, `rtc_time_wake_l2`, `key_wake_l2` |
| 工作负载 | `l3a_idle`, `display_only`, `backlight_levels`, `normal_active`, `imu_sampling`, `ir_tx`, `full_load` |
| Wi-Fi 负载 | `wifi_connected`, `wifi_light_sleep`, `wifi_periodic_traffic` |

在选定的目录内构建。正式测功耗前拔掉 USB Type-C，并且不要运行串口监视器。每个测试报告都链接到对应的测量曲线目录。
