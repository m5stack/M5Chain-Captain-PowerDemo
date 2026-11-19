# M5Chain-Captain-PowerDemo

[English](README.md) | [简体中文](README_CN.md)

## Overview

This repository contains independent ESP-IDF firmware projects for measuring the power consumption of M5Stack Chain Captain (C160, ESP32-S3R8). Each test is built and flashed separately, starts automatically after boot, prints a fixed log contract as evidence, and then holds exactly one operating state so that the current can be measured from the outside. The firmware never measures current itself: the numbers come from an external power analyser and the reports quote its `WINDOW Avg`.

The project is based on ESP-IDF 5.5.5. Build, flash, serial validation and current measurement are manual operations.

> **Full Load safety gate:** `tests/full_load` refuses to run audio, RGB and continuous refresh unless `CONFIG_M5CC_FULL_LOAD_SAFETY_ACK=y`. Review the fixture current limit, thermal limit and stop procedure before enabling it; any of the keys K1 / K2 / K3 stops the load immediately.

Chain Captain has five power states. `3V3_L0` is always on and feeds the PMIC and the RTC; `3V3_L1` feeds the IMU domain through the PMIC LDO; `3V3_L2` feeds the ESP32-S3 and the IO expander through the PMIC DCDC, so the expander is unpowered in L0/L1 and returns with the ESP32 in L2. The L3B enables are additionally gated by `PM_3V3_L2_EN`, which stays off in L0/L1, so those rails cannot be switched on before L2; from L2 on the expander drives the display, codec, IR, boost, header and Grove/Chain rails individually.

## Power Test Results

Power consumption is represented by the average whole-board input current. Unless the linked report says otherwise, each value is the `WINDOW Avg` of an approximately 60-second measurement window.

Device states are abbreviated in this table; use the details links for complete configurations.

| Test scenario | Device / operating state | Avg. current at 3.7 V | Avg. current at 4.2 V | Details |
|---|---|---:|---:|---|
| L0 shutdown | M5PM1: L0 shutdown; charger, boost and LED off<br>RX8130: powered on 3V3_L0; not configured<br>ESP32-S3 / M5IOE1 / BMI270: unpowered | 9.81 µA | 9.66 µA | [Details](tests/l0_shutdown/README.md) |
| L1 standby | M5PM1: L1 standby; LDO hold set<br>RX8130: powered; interrupts disabled<br>BMI270: suspended, advanced power save on<br>ESP32-S3 / M5IOE1: unpowered | 20.87 µA | 20.80 µA | [Details](tests/l1_standby/README.md) |
| L2 deep sleep | ESP32-S3: deep sleep; no wake source<br>M5PM1 / M5IOE1: L2 quiet state; idle sleep 5 s<br>RX8130: powered on 3V3_L0; not configured<br>BMI270: suspended + advanced power save (3V3_L1 retained; power-on default) | 94.36 µA | 91.43 µA | [Details](tests/l2_deep_sleep/README.md) |
| IMU wake from L1 (50 Hz) | M5PM1: L1 standby; G4 wake armed<br>BMI270: any-motion, 50 Hz, INT1 active low<br>RX8130: not configured (powered on 3V3_L0) | 37.48 µA | 37.27 µA | [Details](tests/imu_wake_l1/README.md) |
| IMU wake from L1 (0.78 Hz) | M5PM1: L1 standby; G4 wake armed<br>BMI270: any-motion, 0.78 Hz, INT1 active low<br>RX8130: not configured (powered on 3V3_L0) | 21.02 µA | 20.54 µA | [Details](tests/imu_wake_l1/README.md) |
| IMU wake from L2 (50 Hz) | ESP32-S3: deep sleep; `ext0` on GPIO14<br>M5PM1: G4 wake armed; idle sleep 1 s<br>RX8130: not configured<br>BMI270: any-motion, 50 Hz, INT1 active low | 111.00 µA | 108.55 µA | [Details](tests/imu_wake_l2/README.md) |
| IMU wake from L2 (0.78 Hz) | ESP32-S3: deep sleep; `ext0` on GPIO14<br>M5PM1: G4 wake armed; idle sleep 1 s<br>RX8130: not configured<br>BMI270: any-motion, 0.78 Hz, INT1 active low | 94.48 µA | 92.49 µA | [Details](tests/imu_wake_l2/README.md) |
| RTC alarm wake from L1 | M5PM1: L1 standby; G2 wake armed<br>RX8130: alarm armed, 120 s<br>BMI270: suspended (power-on default; not configured) | 21.35 µA | 21.12 µA | [Details](tests/rtc_alarm_wake_l1/README.md) |
| RTC timer wake from L1 | M5PM1: L1 standby; G2 wake armed<br>RX8130: 120 s countdown timer armed<br>BMI270: suspended (power-on default; not configured) | 21.15 µA | 20.63 µA | [Details](tests/rtc_time_wake_l1/README.md) |
| RTC alarm wake from L2 | ESP32-S3: deep sleep; `ext0` on GPIO14<br>M5PM1: G2 wake; **idle sleep off** (stays awake)<br>RX8130: alarm armed, 120 s<br>BMI270: suspended (power-on default; not configured) | 663.63 µA | 663.87 µA | [Details](tests/rtc_alarm_wake_l2/README.md) |
| RTC timer wake from L2 | ESP32-S3: deep sleep; `ext0` on GPIO14<br>M5PM1: G2 wake; **idle sleep off** (stays awake)<br>RX8130: 120 s countdown timer armed<br>BMI270: suspended (power-on default; not configured) | 665.16 µA | 665.77 µA | [Details](tests/rtc_time_wake_l2/README.md) |
| Key wake from L2 | ESP32-S3: deep sleep; EXT1 on K1 / K2 / K3<br>M5PM1: no wake armed; idle sleep 1 s<br>M5IOE1: quiet state, idle sleep on | 94.21 µA | 91.40 µA | [Details](tests/key_wake_l2/README.md) |
| L3A idle | ESP32-S3: active, 240 MHz, DFS off<br>M5PM1: DCDC on<br>M5IOE1: all L3B off | 35.85 mA | 32.31 mA | [Details](tests/l3a_idle/README.md) |
| Display only | ESP32-S3: active<br>LCD: one static frame, backlight 100 %<br>All other rails off | 99.67 mA | 99.36 mA | [Details](tests/display_only/README.md) |
| Backlight 0 % | ESP32-S3: active<br>LCD: same static frame<br>Backlight: 0 % (LED off, panel powered) | 43.85 mA | 39.51 mA | [Details](tests/backlight_levels/README.md) |
| Backlight 50 % | ESP32-S3: active<br>LCD: same static frame<br>Backlight: 50 % | 75.42 mA | 70.30 mA | [Details](tests/backlight_levels/README.md) |
| Backlight 100 % | ESP32-S3: active<br>LCD: same static frame<br>Backlight: 100 % | 99.69 mA | 99.50 mA | [Details](tests/backlight_levels/README.md) |
| Normal operation | ESP32-S3: active<br>LCD: 50 % backlight, 1 Hz full-screen refresh<br>All other rails off | 75.12 mA | 70.76 mA | [Details](tests/normal_active/README.md) |
| IMU sampling | ESP32-S3: active<br>BMI270: accelerometer + gyroscope at 100 Hz, polled every 10 ms<br>Display off | 37.54 mA | 33.75 mA | [Details](tests/imu_sampling/README.md) |
| IR transmitter | ESP32-S3: active<br>IR LED: NEC frame at 10 Hz, 38 kHz carrier<br>All other L3B off | 39.15 mA | 34.96 mA | [Details](tests/ir_tx/README.md) |
| Full load | ESP32-S3: active<br>LCD: solid white, 100 % backlight, 30 Hz redraw<br>Audio: 1 kHz at maximum volume<br>RGB on, IMU 100 Hz | 458.70 mA | 431.60 mA | [Details](tests/full_load/README.md) |
| Wi-Fi connected | ESP32-S3: active<br>Wi-Fi: associated, `WIFI_PS_NONE`<br>Display off | 109.64 mA | 99.78 mA | [Details](tests/wifi_connected/README.md) |
| Wi-Fi light sleep | ESP32-S3: automatic light sleep<br>Wi-Fi: associated, `WIFI_PS_MAX_MODEM`, listen interval 3 | 6.19 mA | 6.01 mA | [Details](tests/wifi_light_sleep/README.md) |
| Wi-Fi periodic traffic | ESP32-S3: active<br>Wi-Fi: associated, `WIFI_PS_NONE`<br>ICMP: 64 B every 1000 ms | 109.05 mA | 99.94 mA | [Details](tests/wifi_periodic_traffic/README.md) |

Every value was measured with the charger disabled (`CHG_EN = 0`); enabling the charger raises the L0 current by about 5 µA.

## Measurement Graphs

All 3.7 V and 4.2 V captures are collected in the [Power Measurement Graphs](docs/power-graphs/README.md) index, one directory per test item.

## Repository Layout

```text
components/          vendored component snapshot (BSP, PMIC, IO expander, RTC, BMI270, LVGL)
modules/             shared firmware modules (board, power, display, audio, imu, rtc, wifi, test support)
tests/<name>/        one standalone project per test item
docs/power-graphs/   measurement captures, one directory per test item
```

## Build and Flash

ESP-IDF 5.5.5 is required. Build and flash one test from its own directory:

```powershell
cd tests\l3a_idle
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Optional variants are applied by deleting `sdkconfig` and setting `SDKCONFIG_DEFAULTS` to the base file plus one override, for example:

```powershell
Remove-Item -Force sdkconfig -ErrorAction SilentlyContinue
$env:SDKCONFIG_DEFAULTS = "sdkconfig.defaults;sdkconfig.safety-ack.defaults"
idf.py reconfigure
```

The three Wi-Fi tests have an empty SSID, password and target IPv4 by default; fill them in under the `M5CC Power Test` menu of `idf.py menuconfig` (`CONFIG_M5CC_WIFI_SSID` / `CONFIG_M5CC_WIFI_PASSWORD` / `CONFIG_M5CC_WIFI_TARGET_IPV4`). USB Type-C is used for flashing and serial validation only.

## Measurement Method

1. Connect USB Type-C, flash the firmware and confirm `TEST_START`, `BOARD_OK` and `TEST_READY` on the console.
2. Keep the same firmware and configuration, disconnect USB Type-C and power the board only through the measurement setup. The shutdown and sleep tests leave a 3 s window before the state change; the active workloads hold their state indefinitely.
3. For the wake tests, record the average in the armed state before the wake event; the wake itself verifies the path and is not part of the average.
4. Let the current settle, then take an approximately 60 s `WINDOW Avg` at 3.7 V and at 4.2 V.

`wifi_light_sleep` disables the USB Serial/JTAG console once `STATE_ENTERED` has been flushed, so the port may stop responding in that state; this is expected.

## Log Contract

`TEST_START` (test id, firmware version, configuration hash, reset reason), `BOARD_OK` / `BOARD_FAIL`, `TEST_FAIL`, `PROFILE`, `IMU_ARMED`, `RTC_ALARM`, `RTC_TIME`, `TEST_READY`, `STATE_ENTERED`, and for the wake builds `WAKE`, `PMIC_WAKE` and `RTC_WAKE`. The active workloads add `LOAD`, `STOP_HINT`, `STATE_EXIT` and `IMU_SAMPLING`; `SAFETY_BLOCK` marks a safety gate that refused to run the load.

## Dependencies

Vendored under `components/`: M5PM1 1.0.5, M5IOE1 1.0.7, i2c_bus 1.5.2, cmake_utilities 0.5.3, esp_codec_dev 1.5.1, esp_lvgl_port 2.6.2, LVGL 9.5.0, the Chain Captain BSP, plus the RX8130 and BMI270 drivers. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

Project-owned source code is released under the [MIT License](LICENSE). Third-party components remain subject to the licenses in [Third-Party Notices](THIRD_PARTY_NOTICES.md).
