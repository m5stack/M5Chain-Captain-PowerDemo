# Wi-Fi Connected (Power Save Off)

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Measures the current of a station that stays associated to the AP with Wi-Fi power save disabled, so the radio never goes back to sleep. This is the worst-case idle for a connected device.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state
- `m5cc_wifi`: STA connection, power-save mode and periodic ICMP

## Module Configuration

| Module | Configuration |
|---|---|
| ESP32-S3 | STA interface, Wi-Fi power save `WIFI_PS_NONE`, no application traffic, no scan after connecting |
| M5IOE1 | Every L3B enable low, backlight and RGB off, idle sleep enabled |
| M5PM1 | Safe state: charge / boost / LED off, watchdog off, LDO hold set, no wake armed |
| RX8130 | Initialised, alarm and timer interrupts cleared and disabled |
| BMI270 | Left at its power-on default (suspend with advanced power save); the sensor is not configured |
| Firmware log | `BOARD_OK ... WIFI=connected PS=none CHANNEL=<n> RSSI_DBM=<n>` → `STATE_ENTERED mode=wifi_connected ps=none traffic=0 display=0` |

## Runtime Mode

After `TEST_READY` the firmware waits 3 s and then holds the connected state. The console is silent during the measurement window, so the measured current is the association itself.

## Credentials

The SSID, password and target IPv4 are empty by default; fill them in under the `M5CC Power Test` menu of `idf.py menuconfig`. Credentials are never logged.

## Event Task Stack

The Wi-Fi event callback logs the assigned IPv4 address, so `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` is raised from the 2304-byte default to 4096 to avoid a stack overflow in the `sys_evt` task.

## Radio State

`WIFI_PS_NONE` keeps the radio awake between beacons; compare this row against `wifi_light_sleep` to see what power save is worth.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 109.64 mA
- 4.2 V: 99.78 mA

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
| Test ID | `wifi-connected` |
| Config ID | `wifi-connected` |

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

`BOARD_OK ... WIFI=connected PS=none ...` → `TEST_READY` → `STATE_ENTERED mode=wifi_connected ps=none traffic=0 display=0`.
