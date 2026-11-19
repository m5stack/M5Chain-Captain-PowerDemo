# 第三方组件声明

[English](THIRD_PARTY_NOTICES.md) | [简体中文](THIRD_PARTY_NOTICES_CN.md)

本仓库把下列组件原样内置在 `components/` 下，使构建不依赖网络。项目的 MIT 许可证只覆盖本仓库自有源码，
不替代下表中的第三方许可证；内置快照内的许可证文件为准。

| 组件 | 版本 | 上游 | 许可证 | 许可证依据 |
|---|---|---|---|---|
| `m5stack__m5pm1` | 1.0.5 | M5Stack M5PM1 驱动 | MIT | 组件内 `LICENSE` |
| `m5stack__m5ioe1` | 1.0.7 | M5Stack M5IOE1 驱动 | MIT | 组件内 `LICENSE` |
| `espressif__i2c_bus` | 1.5.2 | Espressif 组件仓库 | Apache-2.0 | 组件内 `license.txt` |
| `espressif__cmake_utilities` | 0.5.3 | Espressif 组件仓库 | Apache-2.0 | 组件内 `license.txt` |
| `esp_codec_dev` | 1.5.1 | Espressif 组件仓库 | Apache-2.0 | 组件内 `LICENSE` |
| `esp_lvgl_port` | 2.6.2 | Espressif 组件仓库 | Apache-2.0 | 组件内 `license.txt` |
| `lvgl__lvgl` | 9.5.0 | LVGL | MIT | 组件内 `LICENCE.txt` |
| `m5stack_chain_captain` | 0.0.1 | M5Stack 板级支持包，源自 esp-bsp | Apache-2.0（上游 esp-bsp） | 快照内无许可证文件，适用上游项目许可证 |
| `rx8130` | 快照 | M5Stack RX8130CE 驱动（含数据手册） | 未附独立许可证 | 无许可证文件与 SPDX 头；除 M5Stack 另有说明外按本仓库源码对待 |
| `sensor_bmi270` | 快照 | Bosch Sensortec BMI270 驱动 + M5Stack 的 I2C 封装 | BSD-3-Clause（Bosch 源码） | 每个 Bosch 源文件均带 BSD-3-Clause 头 |

说明：

- `lvgl__lvgl` 已裁剪：删除 `tests/`、`scripts/`、`docs/`，`examples/` 与 `demos/` 保留为空目录占位，
  因为 LVGL 的 ESP CMake 支持会无条件加入这两个路径。库源码与 `LICENCE.txt` 未改动。
- `esp_codec_dev` 与 `esp_lvgl_port` 去掉 `espressif__` 前缀存放，避免组件管理器把它们当作托管依赖。
- `sensor_bmi270` 内含 Bosch Sensortec 源码（`bmi270*.c/h`、`bmi2*.h`），适用其 BSD-3-Clause 条款；
  外层的 `accel_gyro_bmi270` 封装是 M5Stack 代码。
- `rx8130` 快照是 M5Stack 驱动加 RX8130CE 数据手册，没有独立许可证文件。
