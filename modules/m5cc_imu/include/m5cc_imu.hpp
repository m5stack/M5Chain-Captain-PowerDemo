#pragma once

#include <cstdint>

#include "esp_err.h"

namespace m5cc::imu {

struct Result {
    esp_err_t error = ESP_OK;
    int raw_error   = 0;

    [[nodiscard]] bool ok() const
    {
        return error == ESP_OK;
    }
};

enum class WakeProfile : std::uint8_t {
    low_power,
    reference,
};

// Initializes the BMI270 on the shared I2C bus and verifies its chip ID.
[[nodiscard]] Result begin();
bool initialized();

// Turns every sensor off and enables the advanced power-save mode. The register
// state is read back so a failed suspend cannot silently inflate the measurement.
[[nodiscard]] Result suspend();

// Arms ANY_MOTION on INT1 (active low, push-pull, non-latched). Parameters are the
// ones validated by the C160 factory firmware: duration 8 x 20 ms, threshold 50 mg.
//
// low_power  : accelerometer at CONFIG_M5CC_IMU_ACCEL_ODR (50 Hz default) / 2 g /
//              power optimized, gyroscope off
// reference  : accelerometer and gyroscope 100 Hz / performance mode
[[nodiscard]] Result configure_any_motion(WakeProfile profile);
bool any_motion_configured();

// Human readable accelerometer ODR of the low-power wake profile, for PROFILE logs.
const char* accel_odr_label();

// Reads the interrupt status registers, which also clears a latched event.
[[nodiscard]] Result read_wake_status(bool* any_motion);

// Drops any stale latched event so the PMIC wake input starts from a low level.
[[nodiscard]] Result clear_wake_status();

// Enables accelerometer and gyroscope at the requested rate and starts a
// background task that keeps reading samples. Used by the full-load test.
[[nodiscard]] Result start_sampling(std::uint32_t hz);
void stop_sampling();
std::uint32_t sample_count();

}  // namespace m5cc::imu
