# L0 Shutdown

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Enters the L0 shipping state, the deepest state the firmware can reach: the PMIC cuts the ESP32 (and with it the IO expander), the IMU rail and every load switch, and only the always-on 3V3_L0 domain (PMIC and RTC) keeps running.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_power`: L0 / L1 / L2 transitions and wake-source arming
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| M5PM1 | Charge, boost and power LED disabled and read back; GPIO0 / GPIO3 driven low; watchdog off; LDO power hold cleared; `shutdown()` |
| M5IOE1 | Every L3B enable driven low, backlight and RGB off, interrupt output disabled; the expander loses power with 3V3_L2 and the enables are gated by `PM_3V3_L2_EN` |
| RX8130 | Not touched (keeps time on 3V3_L0) |
| BMI270 | Not initialised (3V3_L1 disappears at shutdown) |
| Firmware log | `BOARD_OK PM1=1 IOE=1` |

## Runtime Mode

`TEST_READY` is printed once the board is quiet, then the firmware waits 3 s so that the USB cable can be unplugged before the PMIC shuts the board down.

## Variants

| Override file | `config_id` | Effect |
|---|---|---|
| default | `l0-shutdown` | `CHG_EN = 0` (product configuration) |
| `sdkconfig.charge-on.defaults` | `l0-shutdown` | `CHG_EN = 1`, diagnostic only (`L0_EXPERIMENT charge_en=1` in the log) |

The two builds differ only in the charger bit, which is the last PMIC write before shutdown.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 9.81 µA
- 4.2 V: 9.66 µA

Measured with the charger disabled (`CHG_EN = 0`, product configuration). Enabling the charger raises the L0 current by about 5 µA.

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
| Test ID | `l0-shutdown` |
| Config ID | `l0-shutdown` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

The default build is the product configuration (`CHG_EN = 0`); apply `sdkconfig.charge-on.defaults` for the charger-enabled diagnostic build.

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`TEST_START` with `reset_reason=power_on` → `BOARD_OK PM1=1 IOE=1` → `TEST_READY` → `STATE_ENTERED mode=l0_shutdown ldo=off` → the serial port disappears and does not come back (the board is really off).
