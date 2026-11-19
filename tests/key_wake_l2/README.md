# Key Wake from L2

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Measures the deep-sleep current while the three user keys are the only wake source. The keys are wired straight to the ESP32-S3, so this wake path never touches the PMIC and is only available from L2.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_imu`: BMI270 bring-up, sampling and any-motion arming
- `m5cc_power`: L0 / L1 / L2 transitions and wake-source arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| ESP32-S3 | EXT1 wake on GPIO1 / GPIO4 / GPIO5 (K1 / K2 / K3), any-low, internal pull-ups; PMIC IRQ pin isolated |
| M5PM1 | Safe state: no PMIC wake armed, charge / boost / LED off, watchdog off, LDO hold set, I2C idle sleep 1 s |
| M5IOE1 | Every L3B enable low, backlight and RGB off, idle sleep enabled |
| RX8130 | Initialised, alarm and timer interrupts cleared and disabled |
| BMI270 | Initialised, sensors disabled, advanced power save enabled |
| Firmware log | `STATE_ENTERED mode=key_wake_l2 wake=ext1_k1k2k3 io_sleep=1` → `WAKE cause=ext1 ext1_mask=0x...` |

## Runtime Mode

After `TEST_READY` the firmware waits 3 s, waits (bounded to 10 s) for any key still held from the wake event to be released, arms EXT1 and enters deep sleep. Pressing any key boots the board again; the next boot prints the EXT1 mask before re-arming.

## Wake Path

K1 / K2 / K3 → ESP32-S3 GPIO1 / 4 / 5 EXT1 (any low) → wake. No PMIC pin is involved, so `PMIC_WAKE` must stay at zero on this build.

## Why Not L1

The keys only reach the ESP32; they cannot start the board from a PMIC shutdown. A key-wake L1 test is therefore not possible on this board.

## Wake Evidence

`WAKE cause=ext1 ext1_mask=0x...`: bit 1 is K1 (GPIO1), bit 4 is K2 (GPIO4), bit 5 is K3 (GPIO5), so 0x32 means all three keys were down at the same time.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 94.21 µA
- 4.2 V: 91.40 µA

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
| Test ID | `key-wake-l2` |
| Config ID | `key-wake-l2` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1` → `TEST_READY` → `STATE_ENTERED mode=key_wake_l2 wake=ext1_k1k2k3 io_sleep=1` → the serial port disappears → pressing K1, K2 or K3 reboots the board and prints `WAKE cause=ext1 ext1_mask=0x...`.
