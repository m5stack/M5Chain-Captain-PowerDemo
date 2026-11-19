# L2 Deep Sleep

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Puts the ESP32-S3 into deep sleep with every load switch and service shut down and no wake source armed, so the board only leaves this state after a power cycle.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_imu`: BMI270 bring-up, sampling and any-motion arming
- `m5cc_power`: L0 / L1 / L2 transitions and wake-source arming
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| ESP32-S3 | `esp_deep_sleep_start()` with no wake source armed; ext0 and ext1 disabled |
| M5PM1 | Power LED, charger and boost off; 5 V output low; LDO power hold set (3V3_L1 retained); I2C idle sleep 5 s; IRQ output parked low |
| M5IOE1 | PWM block stopped; LCD, audio, IR, backlight and reset lines off; RGB pins driven low; I2C idle sleep 5 s |
| RX8130 | Not touched; powered on 3V3_L0 and keeps time, interrupts stay at their reset default (disabled) |
| BMI270 | Not initialised; after POR the sensor stays suspended with advanced power save enabled, and 3V3_L1 is retained |
| Firmware log | `STATE_ENTERED mode=l2_deep_sleep` |

## Runtime Mode

`TEST_READY` is printed, then the shutdown sequence runs: load switches and services are switched off, the expander and the PMIC receive their idle-sleep timeouts one second apart, and the chip finally enters deep sleep. `STATE_ENTERED` marks the transition.

## RGB LED Pin Configuration

The three RGB pins (IOE G8 / G9 / G10) are plain push-pull outputs held low, after the expander PWM block is stopped:

```c
ioe->setPwmFrequency(0);
ioe->pinMode(pin, OUTPUT);
ioe->setDriveMode(pin, M5IOE1_DRIVE_PUSHPULL);
ioe->digitalWrite(pin, LOW);
```

A PWM channel left at 0 % duty still conducts, and a floating pin lets the lit LED leak; either adds 150 µA or more, so neither is used here. Tests that need the LED switch the PWM block back on and set a duty cycle.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 94.36 µA
- 4.2 V: 91.43 µA

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
| Test ID | `l2-deep-sleep` |
| Config ID | `l2-deep-sleep` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`TEST_READY` → `STATE_ENTERED mode=l2_deep_sleep` → the serial port disappears and the board stays off until it is power cycled.
