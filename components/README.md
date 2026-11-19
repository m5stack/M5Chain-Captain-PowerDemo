# Vendored Components / 内置组件

本目录内的组件均为**固定版本快照**，用于离线可重现构建。构建时不需要访问 ESP 组件注册表。

All components below are pinned source snapshots so that the project builds offline. No registry access is required.

## 来源与版本 / Provenance

| 组件目录 / Directory | 上游 / Upstream | 版本 / Version | 说明 |
|---|---|---|---|
| `m5stack__m5pm1` | m5stack/M5PM1 | 1.0.5 | PMIC 驱动（`0x6E`） |
| `m5stack__m5ioe1` | m5stack/M5IOE1 | 1.0.7 | IO 扩展器驱动（`0x4F`） |
| `espressif__i2c_bus` | espressif/esp-iot-solution | 1.5.2 | 被 M5PM1 / M5IOE1 的 `REQUIRES` 硬依赖 |
| `espressif__cmake_utilities` | espressif/esp-iot-solution | 0.5.3 | 提供 `package_manager.cmake`（`i2c_bus` 使用） |
| `espressif__esp_codec_dev` → `esp_codec_dev` | espressif/esp-adf | 1.5.1 | 音频 Codec 抽象层（ES8311）；目录去前缀见改动说明 |
| `espressif__esp_lvgl_port` → `esp_lvgl_port` | espressif/esp-bsp | 2.6.2 | LVGL 与 esp_lcd 的对接层；目录去前缀见改动说明 |
| `lvgl__lvgl` | lvgl/lvgl | 9.5.0 | 图形库（已裁剪，见下） |
| `rx8130` | C160 工厂测试工程本地组件 | 未记录 | RX8130CE RTC 驱动 |
| `sensor_bmi270` | Bosch BMI270 Sensor API（C160 工厂测试工程快照） | 未记录 | IMU 驱动 |
| `m5stack_chain_captain` | M5Stack Chain Captain BSP（C160 工厂测试工程本地组件） | 0.0.1 | 板级支持包（LCD/JD9853、背光、音频、L3B 电源轨） |

## 相对上游的改动 / Local modifications

1. **目录重命名**：`espressif__esp_codec_dev` → `esp_codec_dev`、`espressif__esp_lvgl_port` → `esp_lvgl_port`。ESP-IDF 的组件名等于目录名，而 BSP 源码里写的是 `REQUIRES esp_codec_dev` / `esp_lvgl_port`；保留 `espressif__` 前缀会导致找不到组件。名称外的内容未改动。
2. 所有 `idf_component.yml` 删除 `dependencies:` 段，避免构建期解析注册表依赖；依赖关系改由各组件 `CMakeLists.txt` 的 `REQUIRES` / `PRIV_REQUIRES` 表达。
3. 删除各组件自带的 Arduino `examples/` 目录（`m5stack__m5pm1`、`m5stack__m5ioe1`、`esp_lvgl_port`），避免 `.ino` 文件进入 Arduino-Lint 扫描范围。
4. `m5stack_chain_captain`：
   - `CMakeLists.txt` 补齐真实依赖：`esp_driver_i2c/spi/ledc`、`esp_lvgl_port`、`lvgl__lvgl`、`m5stack__m5pm1`、`m5stack__m5ioe1`、`esp_codec_dev`（原依赖由 `idf_component.yml` 的 `public: true` 隐式提供）。
   - 删除 `m5stack_chain_captain.cpp` 中未使用的 `#include "esp_lcd_ili9341.h"`（面板实际使用 ST7789 兼容路径 + JD9853 初始化序列），因此不再需要 `espressif__esp_lcd_ili9341` 组件。
5. `lvgl__lvgl` 裁剪（169.8 MB → 37.0 MB）：删除 `tests/`、`scripts/`、`docs/`、`examples/`、`demos/`、`.devcontainer/` 的内容。其中 `examples/` 与 `demos/` 保留空目录占位（`.gitkeep`），因为 LVGL 的 `env_support/cmake/esp.cmake` 无条件把这两个路径加入 `INCLUDE_DIRS`；LVGL 的 `Kconfig` 只内联定义 Examples/Demos 选项，不做文件级 `source`，因此裁剪不影响配置解析。
6. 删除未使用且架构不匹配的预编译静态库（`esp_codec_dev/device/zl38063/firmware/libfirmware.a`、`lvgl__lvgl/libs/nema_gfx/lib/**/*.a`，共 4.98 MB）。它们只在默认关闭的 `CONFIG_CODEC_ZL38063_SUPPORT` / `LV_USE_NEMA_GFX` 下才会被引用，且面向其他 CPU 架构；同时 `.gitignore` 含 `*.a` 规则，保留会造成仓库与工作区不一致。

## 约束 / Constraints

- 不要在本目录执行 `idf.py` 相关配置；组件由上层 `tests/<name>` 工程通过 `EXTRA_COMPONENT_DIRS` 引入。
- `CONFIG_LV_BUILD_EXAMPLES` 与 `CONFIG_LV_BUILD_DEMOS` 必须保持关闭。`LV_BUILD_DEMOS` 的 Kconfig 默认值是 `y`，若不在 `sdkconfig.defaults` 显式写 `# CONFIG_LV_BUILD_DEMOS is not set`，CMake 会去找已删除的 `demos/lv_demos.c` 并报错。
- 修改此目录内组件时，请在本文件记录改动原因，并保持与后续 `THIRD_PARTY_NOTICES.md` 中的许可证信息一致。
