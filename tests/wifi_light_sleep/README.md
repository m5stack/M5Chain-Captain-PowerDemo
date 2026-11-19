# Wi-Fi with Automatic Light Sleep

[English](README.md) | [简体中文](README_CN.md)

## Purpose

Measures the current of a connected station that uses the Wi-Fi `WIFI_PS_MAX_MODEM` power save together with ESP-IDF automatic light sleep, which is the configuration a battery-powered product would ship with.

## Enabled Modules

- `m5cc_board`: BSP bring-up, rail / GPIO / PWM control and the quiet state
- `m5cc_rtc`: RX8130 time keeping, calendar alarm and countdown timer
- `m5cc_test_support`: test markers, log contract and the safe hold state
- `m5cc_wifi`: STA connection, power-save mode and periodic ICMP

## Module Configuration

| Module | Configuration |
|---|---|
| ESP32-S3 | STA associated, `WIFI_PS_MAX_MODEM`, listen interval from `CONFIG_M5CC_WIFI_LISTEN_INTERVAL`, automatic light sleep with dynamic frequency scaling (80–240 MHz) |
| M5IOE1 | Every L3B enable low, backlight and RGB off, idle sleep enabled |
| M5PM1 | Safe state: charge / boost / LED off, watchdog off, LDO hold set, no wake armed |
| RX8130 | Initialised, alarm and timer interrupts cleared and disabled |
| BMI270 | Left at its power-on default (suspend with advanced power save); the sensor is not configured |
| Firmware log | `BOARD_OK ... PS=max_modem LISTEN_INTERVAL=3 CPU_MIN_MHZ=80 CPU_MAX_MHZ=240 LIGHT_SLEEP=1` → `STATE_ENTERED mode=wifi_light_sleep ps=max_modem light_sleep=1 ...` |

## Runtime Mode

After `TEST_READY` the firmware waits 3 s, prints `STATE_ENTERED`, flushes the console and only then enables automatic light sleep; the CPU wakes only for the beacons the modem has to receive.

## Console in Light Sleep

USB Serial/JTAG stops working once automatic light sleep is enabled. `STATE_ENTERED` is flushed before that, so the last line on the host is the state line; the port may disappear or refuse to reopen (`PermissionError 13` on Windows). This is expected and not a firmware failure. Use the 3 s window after `TEST_READY` for the log, then disconnect USB.

## Power Management

Requires `CONFIG_PM_ENABLE`, `CONFIG_FREERTOS_USE_TICKLESS_IDLE` and `CONFIG_PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP`. The firmware reads the applied `esp_pm_config_t` back and fails the test if the light-sleep flag or the frequency range did not stick.

## Listen Interval

The driver reports the negotiated listen interval; it must equal `CONFIG_M5CC_WIFI_LISTEN_INTERVAL`, otherwise the modem would wake more often than the configuration promises.

## Credentials

The SSID, password and target IPv4 are empty by default; fill them in under the `M5CC Power Test` menu of `idf.py menuconfig`.

## Measured Current

Each value below is the `WINDOW Avg` of the measurement record.

- 3.7 V: 6.19 mA
- 4.2 V: 6.01 mA

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
| Test ID | `wifi-light-sleep` |
| Config ID | `wifi-light-sleep` |

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

`BOARD_OK ... LIGHT_SLEEP=1 ...` → `TEST_READY` → `STATE_ENTERED mode=wifi_light_sleep ps=max_modem light_sleep=1 traffic=0 display=0`; the console stops after that line.
