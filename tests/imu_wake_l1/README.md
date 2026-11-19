# IMU Wake from L1

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Wakes the whole board from the L1 standby state on motion. The BMI270 drives INT1 low, the PMIC sees a falling edge on G4 and powers everything back up.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_imu`: BMI270 bring-up, sampling and any-motion arming
- `m5cc_power`: L0 / L1 / L2 transitions and wake-source arming
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| BMI270 | Any-motion detection, INT1 active low and non-latched, duration about 160 ms, threshold about 50 mg |
| M5PM1 | G4 wake enabled on the falling edge, IRQ unmasked, LDO power hold set, then `shutdown()` |
| M5IOE1 | All L3B enables low before shutdown |
| RX8130 | Not touched; powered on 3V3_L0, interrupts stay at their reset default (disabled) |
| Firmware log | `PROFILE profile=<low_power or reference> accel=<ODR> gyro=<ODR or off> feature=any_motion` → `STATE_ENTERED mode=imu_wake_l1 wake=pmic_g4 ldo_hold=1` |

## Runtime Mode

After arming the motion detector the firmware waits 3 s and shuts the board down. Any movement then restarts the board, which boots and prints the wake evidence before arming again.

## Wake Path

BMI270 INT1 (active low) → PMIC G4 (falling edge) → PMIC powers the board up → ESP32 boots with `reset_reason=power_on`.

## Variants

| Override file | `config_id` | Profile | Accelerometer ODR |
|---|---|---|---|
| default | `imu-wake-l1` | `low_power` | 50 Hz |
| `sdkconfig.odr-lowest.defaults` | `imu-wake-l1` | `low_power` | 0.78 Hz |
| `sdkconfig.profile-reference.defaults` | `imu-wake-l1-reference` | `reference` | 100 Hz + gyroscope |

## Measured Wake Behaviour

Verified on hardware: after the board went dark from shutdown, shaking it brought the board back by itself (`PMIC_WAKE src=0x20`, i.e. the external wake bit), repeating while motion continued.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

| Variant | 3.7 V | 4.2 V |
|---|---|---|
| 50 Hz | 37.48 µA | 37.27 µA |
| 0.78 Hz | 21.02 µA | 20.54 µA |

The 0.78 Hz build keeps the same `low_power` profile and only lowers the accelerometer ODR, so the any-motion duration stays 8 samples and the effective detection time grows to about 10 s of continuous motion.

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
| Test ID | `imu-wake-l1` |
| Config ID | `imu-wake-l1`, `imu-wake-l1-reference` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

The default build is `low_power` at 50 Hz; apply `sdkconfig.odr-lowest.defaults` for 0.78 Hz, or `sdkconfig.profile-reference.defaults` for the reference profile. Both files can be combined with a semicolon.

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`PROFILE profile=low_power accel=50 Hz ...` → `BOARD_OK PM1=1 IOE=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=imu_wake_l1 wake=pmic_g4 ldo_hold=1` → the serial port disappears (shutdown) → after movement the board boots again with `PMIC_WAKE src=0x20` and `WAKE cause=undefined` (a fresh power-on has no ESP32 wake cause).
