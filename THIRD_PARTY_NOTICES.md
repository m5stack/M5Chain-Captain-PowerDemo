# Third-Party Notices

[English](THIRD_PARTY_NOTICES.md) | [简体中文](THIRD_PARTY_NOTICES_CN.md)

This repository vendors the following components as-is under `components/` so that a build does not
depend on the network. The project MIT License applies only to project-owned source code; it does not
replace the licenses below, and the license files inside the vendored snapshots are authoritative.

| Component | Version | Upstream | License | License evidence |
|---|---|---|---|---|
| `m5stack__m5pm1` | 1.0.5 | M5Stack M5PM1 driver | MIT | `LICENSE` in the component |
| `m5stack__m5ioe1` | 1.0.7 | M5Stack M5IOE1 driver | MIT | `LICENSE` in the component |
| `espressif__i2c_bus` | 1.5.2 | Espressif Component Registry | Apache-2.0 | `license.txt` in the component |
| `espressif__cmake_utilities` | 0.5.3 | Espressif Component Registry | Apache-2.0 | `license.txt` in the component |
| `esp_codec_dev` | 1.5.1 | Espressif Component Registry | Apache-2.0 | `LICENSE` in the component |
| `esp_lvgl_port` | 2.6.2 | Espressif Component Registry | Apache-2.0 | `license.txt` in the component |
| `lvgl__lvgl` | 9.5.0 | LVGL | MIT | `LICENCE.txt` in the component |
| `m5stack_chain_captain` | 0.0.1 | M5Stack board support package, derived from esp-bsp | Apache-2.0 (upstream esp-bsp) | No license file in the snapshot; upstream project license applies |
| `rx8130` | snapshot | M5Stack RX8130CE driver (datasheet included) | No separate license file | No license file or SPDX header; treat as part of this repository unless M5Stack states otherwise |
| `sensor_bmi270` | snapshot | Bosch Sensortec BMI270 driver plus an M5Stack I2C wrapper | BSD-3-Clause (Bosch sources) | BSD-3-Clause header in every Bosch source file |

Notes:

- `lvgl__lvgl` has been pruned: `tests/`, `scripts/` and `docs/` were removed and `examples/` and
  `demos/` remain as empty placeholders, because the LVGL ESP CMake support adds those two paths
  unconditionally. The library sources and `LICENCE.txt` are unchanged.
- `esp_codec_dev` and `esp_lvgl_port` are stored without the `espressif__` prefix so that the
  component manager does not treat them as managed dependencies.
- `sensor_bmi270` contains Bosch Sensortec sources (`bmi270*.c/h`, `bmi2*.h`) whose BSD-3-Clause
  terms apply; the thin `accel_gyro_bmi270` wrapper is M5Stack code.
- The `rx8130` snapshot ships the M5Stack driver together with the RX8130CE datasheet and has no
  separate license file.
