# RTC Alarm Wake from L2

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Wakes the ESP32-S3 from deep sleep with the RX8130 through the same G2 → IRQOUT → GPIO14 path used by the IMU, but driven by the RTC instead of motion. The PMIC is deliberately kept awake so that the short /INT pulse is not missed.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_power`: L0 / L1 / L2 transitions and wake-source arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| RX8130 | Reference time written as 2026-01-01 00:00:00 (oscillator stopped first), alarm set 120 s after that reference, alarm interrupt enabled |
| M5PM1 | G2 wake on the falling edge, IRQ unmasked, G1 configured as IRQ output, **I2C idle sleep disabled (PMIC stays awake)** |
| ESP32-S3 | `ext0` wake on GPIO14 at level 0, PMIC IRQ status cleared before sleeping |
| BMI270 | Not initialised; stays suspended with advanced power save at the power-on default on the retained 3V3_L1 rail |
| Firmware log | `RTC_ALARM armed=+120s mode=alarm` → `STATE_ENTERED mode=rtc_alarm_wake_l2 wake=pmic_g2_to_ext0 io_sleep=0` |

## Runtime Mode

`STATE_ENTERED` marks the transition to deep sleep; the alarm fires 120 s later and the chip reboots with `reset_reason=deep_sleep`.

## Wake Path

RX8130 /INT → PMIC G2 → PMG1_IRQOUT low → ESP32 GPIO14 `ext0` → `wake_cause=ext0`.

## PMIC Stays Awake

The RX8130 /INT pulse is too short for the PMIC I2C idle-sleep cycle to catch, so this test calls `finalize_l2(false)`: the expander may idle-sleep, the PMIC must not. That is why the current is in the hundreds of microamps instead of the ~94 µA L2 baseline. Do not enable PMIC idle sleep here to lower the reading; the alarm would no longer wake the board.

## Measurement Window

The board wakes, re-arms and sleeps again every 120 s. A capture that contains one wake burst reports the average of that cycle (about 664 µA in this round), not a pure idle value. Take the window within the first 100 s after `STATE_ENTERED`, or re-time the alarm for a longer period.

## Measured Wake Behaviour

Verified on hardware: the board re-enumerated by itself on the alarm (about 115 s after the state line), which is only possible if the alarm woke the chip.

## Separate Project

The countdown-timer variant is its own project, `tests/rtc_time_wake_l2`, exactly like the reference power-demo layout.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 663.63 µA
- 4.2 V: 663.87 µA

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
| Test ID | `rtc-alarm-wake-l2` |
| Config ID | `rtc-alarm-wake-l2` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`RTC_ALARM armed=+120s mode=alarm` → `STATE_ENTERED mode=rtc_alarm_wake_l2 wake=pmic_g2_to_ext0 io_sleep=0` → the serial port disappears → after the alarm the chip reboots with `reset_reason=deep_sleep wake_cause=ext0`.
