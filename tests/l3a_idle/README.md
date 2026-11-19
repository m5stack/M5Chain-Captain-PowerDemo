# L3A Idle

[English](README.md) | [简体中文](README_CN.md)

## Purpose

This is the active baseline: the ESP32-S3 runs at 240 MHz with every L3B rail switched off, so the measurement isolates the ESP32 plus the always-on domain.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_power`: L0 / L1 / L2 transitions and wake-source arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| ESP32-S3 | 240 MHz, DFS off, no application load after the state line |
| M5PM1 | Charge, boost and LED disabled; LDO hold; DCDC stays on |
| M5IOE1 | All L3B enables low, interrupt output disabled |
| RX8130 | Initialised, interrupts cleared and disabled |
| BMI270 | Not initialised; stays suspended with advanced power save at the power-on default |
| Firmware log | `STATE_ENTERED mode=l3a_idle cpu_mhz=240 dfs=off` |

## Runtime Mode

After `STATE_ENTERED` the firmware only blocks in `vTaskDelay(portMAX_DELAY)`, so no task wakes up periodically and the console stays silent.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 35.85 mA
- 4.2 V: 32.31 mA

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
| Test ID | `l3a-idle` |
| Config ID | `l3a-idle` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`BOARD_OK PM1=1 IOE=1 RTC=1` → `TEST_READY` → `STATE_ENTERED mode=l3a_idle cpu_mhz=240 dfs=off`, then silence.
