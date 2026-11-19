# IMU Wake from L2

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Wakes the ESP32-S3 from deep sleep on motion: the BMI270 drives INT1 low, the PMIC forwards it through IRQOUT to GPIO14 and the `ext0` wake source restarts the chip.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_imu`: BMI270 bring-up, sampling and any-motion arming
- `m5cc_power`: L0 / L1 / L2 transitions and wake-source arming
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| BMI270 | Any-motion detection, INT1 active low and non-latched, duration about 160 ms, threshold about 50 mg |
| M5PM1 | G4 wake on the falling edge, IRQ unmasked, G1 configured as the IRQ output, I2C idle sleep 1 s |
| ESP32-S3 | `ext0` wake on GPIO14 at level 0, PMIC IRQ status cleared before sleeping |
| RX8130 | Not touched; powered on 3V3_L0, interrupts stay at their reset default (disabled) |
| Firmware log | `STATE_ENTERED mode=imu_wake_l2 wake=pmic_g4_to_ext0 io_sleep=1` |

## Runtime Mode

`STATE_ENTERED` marks the moment the chip is about to sleep. After motion the chip reboots with `reset_reason=deep_sleep`.

## Wake Path

BMI270 INT1 (active low) → PMIC G4 → PMG1_IRQOUT low → ESP32 GPIO14 `ext0` → `wake_cause=ext0`.

## Variants

| Override file | `config_id` | Profile | Accelerometer ODR |
|---|---|---|---|
| default | `imu-wake-l2` | `low_power` | 50 Hz |
| `sdkconfig.odr-lowest.defaults` | `imu-wake-l2` | `low_power` | 0.78 Hz |
| `sdkconfig.profile-reference.defaults` | `imu-wake-l2-reference` | `reference` | 100 Hz + gyroscope |

## Measured Wake Behaviour

Verified on hardware: 40 s without movement produced no wake at all, and shaking the board woke it with `reset_reason=deep_sleep` and `wake_cause=ext0`.

## Capture Note

Open the port with a tool that does not touch RTS / DTR. `.NET SerialPort` asserts them on open, which resets the chip and replaces `wake_cause` with `usb`.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

| Variant | 3.7 V | 4.2 V |
|---|---|---|
| 50 Hz | 111.00 µA | 108.55 µA |
| 0.78 Hz | 94.48 µA | 92.49 µA |

The 0.78 Hz build keeps the same `low_power` profile and only lowers the accelerometer ODR; at that rate the IMU contribution drops to the noise level of the L2 baseline.

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
| Test ID | `imu-wake-l2` |
| Config ID | `imu-wake-l2`, `imu-wake-l2-reference` |

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

`PROFILE profile=low_power accel=50 Hz ...` → `BOARD_OK PM1=1 IOE=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=imu_wake_l2 wake=pmic_g4_to_ext0 io_sleep=1` → after movement `TEST_START` with `reset_reason=deep_sleep wake_cause=ext0`.
