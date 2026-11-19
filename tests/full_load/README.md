# Full Load

[English](README.md) | [简体中文](README_CN.md)

## Purpose

This test applies the worst load the board can generate in software: a solid white LCD at 100 % with a 30 Hz full-screen redraw, a 1 kHz tone at maximum volume, the RGB LED on and 100 Hz IMU sampling.

## Enabled Modules

- `m5cc_audio`: ES8311 codec and AW8737A PA bring-up, tone streaming
- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_display`: JD9853 + LVGL bring-up, static and solid frames, continuous redraw
- `m5cc_imu`: BMI270 bring-up, sampling and any-motion arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| Display | Backlight 100 %, solid white frame (`show_solid_white`) redrawn at 30 Hz |
| Audio | ES8311 + AW8737A (PA mode 4), mono 22050 Hz, volume 100, 1 kHz sine at about −2 dBFS, streamed from its own task |
| RGB | IOE PWM channels 4 / 2 / 1 set to 70 % / 100 % / 100 % (red / green / blue) |
| BMI270 | Accelerometer and gyroscope sampling at 100 Hz |
| Buttons | K1 / K2 / K3 polled; any key de-loads audio and drops the backlight to 50 % |
| Firmware log | `LOAD lcd=1 audio=1 rgb=1 imu=1 ...` → `STATE_ENTERED mode=full_load` → `STOP_HINT press_any_key=stop max_load=<n>s` |

## Runtime Mode

The load is applied after `TEST_READY` and runs until it is stopped by any of the keys K1 / K2 / K3 or by the automatic timeout (`CONFIG_M5CC_FULL_LOAD_MAX_SECONDS`, 600 s by default). On stop the firmware prints `STATE_EXIT reason=key` or `reason=timeout` and keeps the backlight at 50 %.

## Safety Gate

Audio, RGB and continuous redraw stay off unless `CONFIG_M5CC_FULL_LOAD_SAFETY_ACK=y`. Without it the firmware prints `SAFETY_BLOCK reason=full_load_not_acked` and parks in the L3A idle state. Enable it only after the fixture current limit and thermal behaviour have been reviewed.

## Variants

| Override file | `config_id` | Effect |
|---|---|---|
| default | `full-load` | gate closed, prints `SAFETY_BLOCK` |
| `sdkconfig.safety-ack.defaults` | `full-load` | gate open, runs the full load |

## White Panel

The panel shows a flat white frame instead of the text frame, so the LCD panel and the backlight are both at maximum load; the 30 Hz full-screen redraw then adds the SPI traffic of the active case.

## Stop Procedure

Press K1, K2 or K3 to unload immediately. The maximum runtime is a fallback for an unattended board; the safety gate is independent of both.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 458.70 mA
- 4.2 V: 431.60 mA

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
| Test ID | `full-load` |
| Config ID | `full-load` |

## Build and Flash

This test cannot run in its default configuration; apply the required override file before configuring and delete `sdkconfig` first:

```powershell
Remove-Item -Force sdkconfig -ErrorAction SilentlyContinue
$env:SDKCONFIG_DEFAULTS = "sdkconfig.defaults;sdkconfig.safety-ack.defaults"
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

The default build prints `SAFETY_BLOCK`; the safety-ack override is required for a real load run.

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `LOAD lcd=1 audio=1 rgb=1 imu=1 brightness=100 volume=100 tone_hz=1000 draw_hz=30` → `STATE_ENTERED mode=full_load` → `STOP_HINT press_any_key=stop max_load=600s`. The screen is solid white, the RGB LED is lit and the speaker produces an audible tone; any key prints `STATE_EXIT reason=key`.
