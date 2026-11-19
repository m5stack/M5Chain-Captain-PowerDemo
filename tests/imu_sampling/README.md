# IMU Sampling

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Measures the active-board current while the BMI270 streams accelerometer and gyroscope data continuously, with the display, audio, RGB and every other load switched off. This isolates the sensor and its I2C workload.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_imu`: BMI270 bring-up, sampling and any-motion arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| BMI270 | Accelerometer and gyroscope at 100 Hz, ±2 g / ±2000 dps, performance mode, advanced power save off; polled every 10 ms by a background task |
| M5PM1 | Charge, boost and power LED disabled; Grove / Chain 5 V off |
| M5IOE1 | Every L3B enable low, backlight off, RGB off, interrupt output disabled |
| RX8130 | Initialised, alarm and timer interrupts cleared and disabled |
| Display and audio | Not started (no backlight, no codec rail, no I2S traffic) |
| Firmware log | `IMU_SAMPLING hz=100 samples=<n>` → `STATE_ENTERED mode=imu_sampling accel_hz=100 gyro_hz=100 display=off` |

## Runtime Mode

After `TEST_READY` the firmware enables the sensors, starts the sampling task and prints `IMU_SAMPLING`; the task keeps polling the chip for the whole measurement window while the main task only sleeps.

## Fixed Configuration

This test has one fixed configuration (100 Hz accelerometer + gyroscope, sampled every 10 ms). It does not provide an optional mode.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 37.54 mA
- 4.2 V: 33.75 mA

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
| Test ID | `imu-sampling` |
| Config ID | `imu-sampling` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `IMU_SAMPLING hz=100 samples=<n>` → `STATE_ENTERED mode=imu_sampling accel_hz=100 gyro_hz=100 display=off`; `<n>` keeps growing if the test is re-run.
