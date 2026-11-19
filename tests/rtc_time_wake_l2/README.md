# RTC Timer Wake from L2

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Wakes the ESP32-S3 from deep sleep with the RX8130 countdown timer through the same G2 → IRQOUT → GPIO14 path used by the alarm and the IMU. The PMIC is deliberately kept awake so that the short /INT pulse is not missed.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_power`: L0 / L1 / L2 transitions and wake-source arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| RX8130 | 120 s countdown timer armed through the timer interrupt; the calendar alarm is not used |
| M5PM1 | G2 wake on the falling edge, IRQ unmasked, G1 configured as IRQ output, **I2C idle sleep disabled (PMIC stays awake)** |
| ESP32-S3 | `ext0` wake on GPIO14 at level 0, PMIC IRQ status cleared before sleeping |
| BMI270 | Not initialised; stays suspended with advanced power save at the power-on default on the retained 3V3_L1 rail |
| Firmware log | `RTC_ALARM armed=+120s mode=timer` → `STATE_ENTERED mode=rtc_time_wake_l2 wake=pmic_g2_to_ext0 io_sleep=0` |

## Runtime Mode

`STATE_ENTERED` marks the transition to deep sleep; the timer expires 120 s later and the chip reboots with `reset_reason=deep_sleep`.

## Wake Path

RX8130 /INT → PMIC G2 → PMG1_IRQOUT low → ESP32 GPIO14 `ext0` → `wake_cause=ext0`.

## PMIC Stays Awake

The RX8130 /INT pulse is too short for the PMIC I2C idle-sleep cycle to catch, so this test calls `finalize_l2(false)`: the expander may idle-sleep, the PMIC must not. That is why the current is in the hundreds of microamps instead of the ~94 µA L2 baseline.

## Measurement Window

The timer wakes the chip, which re-arms and sleeps again every 120 s. A capture that contains one wake burst reports the average of that cycle (about 665 µA), not a pure idle value. Take the window within the first 100 s after `STATE_ENTERED`, or re-time the timer for a longer period.

## One-shot Timer

The countdown timer fires once. A missed wake event needs a reset to re-arm, not a re-flash.

## Separate Project

The calendar-alarm variant is its own project, `tests/rtc_alarm_wake_l2`, matching the reference power-demo layout.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 665.16 µA
- 4.2 V: 665.77 µA

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
| Test ID | `rtc-time-wake-l2` |
| Config ID | `rtc-time-wake-l2` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`RTC_ALARM armed=+120s mode=timer` → `STATE_ENTERED mode=rtc_time_wake_l2 wake=pmic_g2_to_ext0 io_sleep=0` → the serial port disappears → after the timer the chip reboots with `reset_reason=deep_sleep wake_cause=ext0` and `RTC_WAKE timer=1`.
