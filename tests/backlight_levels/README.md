# Backlight Levels

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Measures the whole-board current of the same static TFT frame at three backlight levels (0 %, 50 % and 100 %). The display traffic is identical in all three builds, so the differences come from the backlight alone.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_display`: JD9853 + LVGL bring-up, static and solid frames, continuous redraw
- `m5cc_imu`: BMI270 bring-up, sampling and any-motion arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| M5IOE1 | TFT rail enabled; backlight enable high with the configured PWM duty (IOE G11, PWM channel 3, 20 kHz); every other L3B enable low, RGB off |
| M5PM1 | Charge, boost and power LED disabled; Grove / Chain 5 V off; watchdog off |
| RX8130 | Initialised, alarm and timer interrupts cleared and disabled |
| BMI270 | Initialised, sensors disabled, advanced power save enabled |
| Firmware log | `BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1 BACKLIGHT=<level>` → `STATE_ENTERED mode=backlight_levels brightness=<level> refresh_hz=0` |

## Runtime Mode

The firmware draws the same static frame as `display_only`, then applies the level selected at build time. Nothing on screen changes during the capture, so the only difference between the three builds is the backlight duty cycle.

## Operating Modes

| `config_id` | Override file | Backlight |
|---|---|---:|
| `backlight-0` | `sdkconfig.backlight-0.defaults` | 0 % |
| `backlight-50` | `sdkconfig.backlight-50.defaults` | 50 % |
| `backlight-100` | `sdkconfig.backlight-100.defaults` | 100 % |

The backlight level is a percentage written to IOE PWM channel 3 (`CONFIG_M5CC_BACKLIGHT_PERCENT`); there is no 12-bit duty register in this path.

## Cross-check

The 100 % build is functionally identical to `display_only`; measuring both is a deliberate check that the two firmware paths agree.

## 0 % Is Not Off

At 0 % the TFT rail and the panel logic stay powered; only the backlight LED current is removed. This answers whether the backlight enable pin really switches the LED string off.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

| Variant | 3.7 V | 4.2 V |
|---|---|---|
| 0 % | 43.85 mA | 39.51 mA |
| 50 % | 75.42 mA | 70.30 mA |
| 100 % | 99.69 mA | 99.50 mA |

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
| Test ID | `backlight-levels` |
| Config ID | `backlight-levels`, `backlight-0`, `backlight-50`, `backlight-100` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

The default build is 50 %. Apply `sdkconfig.backlight-0.defaults` or `sdkconfig.backlight-100.defaults` for the other two levels.

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`BOARD_OK ... BACKLIGHT=<level>` → `TEST_READY` → `STATE_ENTERED mode=backlight_levels brightness=<level> refresh_hz=0`; the screen stays still and the backlight visibly matches the level.
