# Normal Active

[English](README.md) | [简体中文](README_CN.md)

## Purpose

The realistic working state: LCD on with the backlight at 50 % and a full-screen refresh at 1 Hz, everything else powered down.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_display`: JD9853 + LVGL bring-up, static and solid frames, continuous redraw
- `m5cc_imu`: BMI270 bring-up, sampling and any-motion arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| Display | JD9853 panel through the factory BSP and LVGL 9, backlight 50 % (IOE PWM channel 3, 20 kHz, duty = 50 %), full-screen redraw every second |
| M5PM1 | Charge, boost and LED disabled; LDO hold; DCDC on |
| M5IOE1 | Only the LCD rail and backlight are enabled; audio, RGB, IR, boost, header and Grove / Chain rails stay off |
| BMI270 / RX8130 | BMI270 suspended, RTC interrupts disabled |
| Firmware log | `STATE_ENTERED mode=normal_active brightness=50 refresh_hz=1` |

## Runtime Mode

The display stack is brought up after `TEST_READY`; `STATE_ENTERED` is printed once the first frame has been pushed, then the 1 Hz refresh continues silently.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 75.12 mA
- 4.2 V: 70.76 mA

See the [power measurement graph index](../../docs/power-graphs/README.md).

## Configuration

| Setting | Value |
|---|---|
| Target | ESP32-S3R8 (16 MB flash, 8 MB octal PSRAM) |
| Flash | 16 MB |
| PSRAM | Octal, 80 MHz |
| CPU | 240 MHz, no DFS |
| Console | USB Serial/JTAG |
| I2C | 100 kHz |
| Bus devices | M5PM1 0x6E, M5IOE1 0x4F |
| Test ID | `normal-active` |
| Config ID | `normal-active` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=normal_active brightness=50 refresh_hz=1`, and the panel keeps updating.
