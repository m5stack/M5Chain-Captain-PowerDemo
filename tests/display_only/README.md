# Display Only

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Measures the whole-board current with the TFT powered at full brightness showing one static frame, while every other peripheral load is off. This isolates the panel, its rail and the backlight from the CPU-only baseline.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_display`: JD9853 + LVGL bring-up, static and solid frames, continuous redraw
- `m5cc_imu`: BMI270 bring-up, sampling and any-motion arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| M5IOE1 | TFT rail enabled; backlight enable high with PWM duty 100 %; every other L3B enable low, RGB off, interrupt output disabled |
| M5PM1 | Charge, boost and power LED disabled; Grove / Chain 5 V off; watchdog off |
| RX8130 | Initialised, alarm and timer interrupts cleared and disabled |
| BMI270 | Initialised, sensors disabled, advanced power save enabled |
| Firmware log | `STATE_ENTERED mode=display_only brightness=100 refresh_hz=0` |

## Runtime Mode

After `TEST_READY` the firmware waits 3 s, powers the TFT rail, starts the JD9853 panel and LVGL, sets the backlight to 100 % and draws one static frame. No timer or draw task runs afterwards, so the measurement window contains a still image and no periodic SPI traffic.

## Static Frame

The panel is refreshed once before `STATE_ENTERED`. Leaving the image unchanged is what separates this test from `normal_active`, which invalidates the whole screen once per second.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 99.67 mA
- 4.2 V: 99.36 mA

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
| Test ID | `display-only` |
| Config ID | `display-only` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=display_only brightness=100 refresh_hz=0`; the screen keeps a still frame and the serial port stays up.
