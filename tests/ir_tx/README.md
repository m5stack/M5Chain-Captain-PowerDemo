# IR Transmitter

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Measures the active current while the infrared LED transmits NEC frames. The IR rail is the only L3B load in this build, so the difference against L3A idle is the transmitter cost.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state

## Module Configuration

| Module | Configuration |
|---|---|
| ESP32-S3 | RMT TX channel on GPIO48, 1 µs resolution, 38 kHz carrier at 33 % duty, one NEC frame every 100 ms |
| M5IOE1 | IR rail (`IOEG2_IR_L3B_EN`) high; backlight, RGB and every other L3B enable low |
| M5PM1 | Safe state: charge / boost / LED off, watchdog off, LDO hold set, no wake armed |
| RX8130 | Initialised, alarm and timer interrupts cleared and disabled |
| BMI270 | Left at its power-on default (suspend with advanced power save); the sensor is not configured |
| Firmware log | `BOARD_OK ... IR=nec carrier_hz=38000 duty=0.33 frames_per_s=10` → `STATE_ENTERED mode=ir_tx nec_38khz_10hz display=0` |

## Runtime Mode

After `TEST_READY` the firmware waits 3 s, then transmits one NEC frame (address 0x00, command 0x00) every 100 ms for as long as the board stays powered.

## Frame

9 ms leader mark, 4.5 ms leader space, 32 data bits at 560 µs / 560 µs (0) or 560 µs / 1690 µs (1), then the stop mark. The inter-frame gap comes from the transmit loop, not from an over-long RMT duration.

## Pins

IR TX = GPIO48. The IR receiver (GPIO47) is not referenced by this project.

## Why This Load

A permanent 38 kHz carrier would leave the LED continuously on; ten NEC frames per second keeps the load representative of a real remote control.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 39.15 mA
- 4.2 V: 34.96 mA

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
| Test ID | `ir-tx` |
| Config ID | `ir-tx` |

## Build and Flash

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`BOARD_OK ... IR=nec ...` → `TEST_READY` → `STATE_ENTERED mode=ir_tx nec_38khz_10hz display=0`.
