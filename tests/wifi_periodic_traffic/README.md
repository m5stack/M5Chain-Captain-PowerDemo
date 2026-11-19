# Wi-Fi Periodic Traffic

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Measures the current of a connected station that sends one 64-byte ICMP echo per second, which represents a device that keeps a light heartbeat with a server.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state
- `m5cc_wifi`: STA connection, power-save mode and periodic ICMP

## Module Configuration

| Module | Configuration |
|---|---|
| ESP32-S3 | STA associated, `WIFI_PS_NONE`, one 64-byte ICMP echo every 1000 ms to the configured IPv4 target |
| M5IOE1 | Every L3B enable low, backlight and RGB off, idle sleep enabled |
| M5PM1 | Safe state: charge / boost / LED off, watchdog off, LDO hold set, no wake armed |
| RX8130 | Initialised, alarm and timer interrupts cleared and disabled |
| BMI270 | Left at its power-on default (suspend with advanced power save); the sensor is not configured |
| Firmware log | `BOARD_OK ... WIFI=connected PS=none PING_MS=1000 PAYLOAD=64` → `STATE_ENTERED mode=wifi_periodic_traffic ps=none ping_1hz=64B display=0` |

## Runtime Mode

After `TEST_READY` the firmware waits 3 s, starts the traffic task and keeps pinging once per second; the console only reports a skipped attempt if the echo never left the chip.

## Target

`CONFIG_M5CC_WIFI_TARGET_IPV4` must hold a numeric IPv4 address; host names are never resolved so the measurement contains no DNS traffic.

## Payload

The ping payload is fixed at 64 bytes by the Wi-Fi module, matching the plan in section 5.3.

## Credentials

The SSID, password and target IPv4 are empty by default; fill them in under the `M5CC Power Test` menu of `idf.py menuconfig`.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 109.05 mA
- 4.2 V: 99.94 mA

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
| Test ID | `wifi-periodic-traffic` |
| Config ID | `wifi-periodic-traffic` |

## Build and Flash

Fill in the Wi-Fi fields before building:

```powershell
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

In `menuconfig` open `M5CC Power Test` and set the SSID, password and target IPv4. The defaults are empty, so the test cannot connect until they are filled in.

Requires ESP-IDF v5.5.5. Each project ships its own `sdkconfig.defaults`; do not copy another project's `sdkconfig` over it. Disconnect USB Type-C before the measurement and do not run the monitor while the current is captured.

## Expected Validation

`BOARD_OK ... WIFI=connected PS=none PING_MS=1000 PAYLOAD=64` → `TEST_READY` → `STATE_ENTERED mode=wifi_periodic_traffic ps=none ping_1hz=64B display=0`.
