# Power Test Projects

[English](README.md) | [简体中文](README_CN.md)

Each directory is a standalone project built with ESP-IDF 5.5.5, covering one M5Stack Chain Captain operating state or workload. The directory name is the test identifier used by the firmware and its measurement evidence.

| Test group | Directories |
|---|---|
| Low-power states | `l0_shutdown`, `l1_standby`, `l2_deep_sleep` |
| Wake sources | `imu_wake_l1`, `imu_wake_l2`, `rtc_alarm_wake_l1`, `rtc_time_wake_l1`, `rtc_alarm_wake_l2`, `rtc_time_wake_l2`, `key_wake_l2` |
| Operating workloads | `l3a_idle`, `display_only`, `backlight_levels`, `normal_active`, `imu_sampling`, `ir_tx`, `full_load` |
| Wi-Fi workloads | `wifi_connected`, `wifi_light_sleep`, `wifi_periodic_traffic` |

Build from the selected directory. Before formal power measurement, disconnect USB Type-C and do not run the serial monitor. Each test report links to its corresponding measurement graph directory.
