# RTC Timer Wake from L1

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Wakes the whole board from L1 standby with the RX8130 countdown timer. The timer asserts /INT, the PMIC sees a falling edge on G2 and powers the board up.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_power`: L0 / L1 / L2 transitions and wake-source arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| RX8130 | 120 s countdown timer armed through the timer interrupt; the calendar alarm is not used |
| M5PM1 | G2 wake on the falling edge, IRQ unmasked, LDO power hold set, then `shutdown()` |
| M5IOE1 | All L3B enables low before shutdown |
| BMI270 | Not initialised; stays suspended with advanced power save at the power-on default on the retained 3V3_L1 rail |
| Firmware log | `RTC_ALARM armed=+120s mode=timer` → `STATE_ENTERED mode=rtc_time_wake_l1 wake=pmic_g2 timer=120s ldo_hold=1` |

## Runtime Mode

`RTC_ALARM armed=+120s mode=timer` is printed after arming, then the board shuts down. It powers itself back up once when the timer expires and arms again.

## Wake Path

RX8130 /INT (open drain, active low) → PMIC G2 falling edge → board power-up → ESP32 boots with `reset_reason=power_on`.

## One-shot Timer

Unlike the calendar alarm, the countdown timer fires once. If the wake event is missed, reset the board to re-arm; no re-flash is needed.

## Separate Project

The calendar-alarm variant is its own project, `tests/rtc_alarm_wake_l1`, matching the reference power-demo layout.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 21.15 µA
- 4.2 V: 20.63 µA

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
| Test ID | `rtc-time-wake-l1` |
| Config ID | `rtc-time-wake-l1` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`RTC_ALARM armed=+120s mode=timer` → `STATE_ENTERED mode=rtc_time_wake_l1` → the serial port disappears → the board boots again with `PMIC_WAKE src=0x20` and `RTC_WAKE alarm=0 timer=1`.
