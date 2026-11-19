#pragma once

#include <cstdint>

#include "esp_err.h"

namespace m5cc::rtc {

struct Result {
    esp_err_t error = ESP_OK;
    int raw_error   = 0;

    [[nodiscard]] bool ok() const
    {
        return error == ESP_OK;
    }
};

struct WakeFlags {
    bool alarm = false;
    bool timer = false;
};

// Initializes the RX8130CE on the shared I2C bus. The interrupt pin is not used
// on this board build; wake routing goes through the PMIC.
[[nodiscard]] Result begin();
bool initialized();

// Clears alarm and timer interrupt enables, stops the countdown timer, clears
// pending flags, and reads the control registers back.
[[nodiscard]] Result disable_interrupts();

// Arms the daily alarm so that it fires `seconds` from now. The alarm register
// only matches whole minutes, so the reference time is placed such that the next
// minute boundary is exactly that far away (30 s is supported).
[[nodiscard]] Result arm_alarm(std::uint32_t seconds);

// Arms the countdown timer to fire `seconds` from now. Whole minutes use the
// 1/60 Hz clock; other values use 1 Hz (max 255 s).
[[nodiscard]] Result arm_timer(std::uint32_t seconds);

// Reads the alarm / timer flags without clearing them.
[[nodiscard]] Result read_wake_flags(WakeFlags* flags);

[[nodiscard]] Result clear_wake_flags();

}  // namespace m5cc::rtc
