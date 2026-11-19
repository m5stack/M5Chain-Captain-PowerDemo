# L1 Standby

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Shuts the board down while deliberately keeping the 3V3_L1 IMU domain alive, which is the state a motion-wake build waits in. No wake source is armed, so the only way out is the power button.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_imu`: BMI270 bring-up, sampling and any-motion arming
- `m5cc_power`: L0 / L1 / L2 transitions and wake-source arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| M5PM1 | Charge, boost and LED disabled; LDO power hold **set**; GPIO0 / GPIO3 low; watchdog off; `shutdown()` |
| M5IOE1 | All L3B enables low, interrupt output disabled; loses power with 3V3_L2 at shutdown and the enables are gated by `PM_3V3_L2_EN` |
| RX8130 | Initialised, alarm and timer interrupts cleared and disabled |
| BMI270 | Initialised, sensors disabled, advanced power save enabled |
| Firmware log | `BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` |

## Runtime Mode

After `TEST_READY` the firmware waits 3 s, prints `STATE_ENTERED`, arms the LDO hold and calls `shutdown()`; the ESP32 and every load switch are off while 3V3_L1 stays on.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 20.87 µA
- 4.2 V: 20.80 µA

Measured with the charger disabled, like every other result in this repository. Enabling the charger raises the current by about 5 µA.

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
| Test ID | `l1-standby` |
| Config ID | `l1-standby` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=l1_standby wake=none ldo_hold=1` → the serial port disappears and does not come back.
