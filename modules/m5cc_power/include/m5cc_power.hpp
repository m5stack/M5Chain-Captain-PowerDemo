#pragma once

#include <cstdint>

#include "esp_err.h"

namespace m5cc::power {

enum class WakeSource : std::uint8_t {
    none = 0,
    imu,
    rtc,
    // ESP32-S3 EXT1 on the three user keys (K1 GPIO1 / K2 GPIO4 / K3 GPIO5).
    // Keys are wired straight to the ESP32, so this source is L2 (deep sleep)
    // only: it cannot wake the board from a PMIC shutdown.
    key,
};

struct Result {
    esp_err_t error = ESP_OK;
    int raw_error   = 0;

    [[nodiscard]] bool ok() const
    {
        return error == ESP_OK;
    }
};

// L0: PMIC shutdown with the 3V3_L1 rail released. Never returns on success.
[[nodiscard]] Result enter_l0(bool keep_charge_enabled = false);

// L1: PMIC shutdown with the 3V3_L1 rail held for the IMU / RTC domain.
// Never returns on success.
[[nodiscard]] Result enter_l1(WakeSource wake = WakeSource::none);

// L2 preparation: rails, PMIC wake routing and the ESP32 sleep source.
[[nodiscard]] Result prepare_l2(WakeSource wake = WakeSource::none);

// Last I2C transaction before Deep Sleep: IOE and PM1 idle-sleep timeouts.
// pm1_idle_sleep=false keeps the PMIC awake so it can report wake edges in L2.
[[nodiscard]] Result finalize_l2(bool pm1_idle_sleep = true);

// Settles, then enters Deep Sleep. Never returns.
[[noreturn]] void enter_deep_sleep(std::uint32_t settle_ms = 1000);

// Wake evidence. Both are read-only and do not clear pending flags.
[[nodiscard]] Result read_pmic_wake(std::uint8_t* wake_source, std::uint8_t* gpio_irq);
[[nodiscard]] Result clear_pmic_wake();

}  // namespace m5cc::power
