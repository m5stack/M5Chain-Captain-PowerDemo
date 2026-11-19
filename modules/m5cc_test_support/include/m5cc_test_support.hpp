#pragma once

#include <cstdint>

#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_system.h"

namespace m5cc::test_support {

// IMU wake sensor load selected at build time (CONFIG_M5CC_IMU_WAKE_PROFILE).
enum class Profile : std::uint8_t {
    none = 0,
    low_power,
    reference,
};

// Firmware-side result only. Never a product verdict.
enum class FinalStatus : std::uint8_t {
    pass,
    fail,
};

// Emits TEST_START. Call once, before any peripheral initialization.
esp_err_t initialize(Profile profile = Profile::none);

// Emits TEST_READY and flushes the console. The firmware must stay silent after
// this call so the measured window contains no console or bus activity.
esp_err_t mark_ready();

// Emits STATE_ENTERED. `extra` is an optional "key=value key=value" tail.
void state_entered(const char* mode, const char* extra = nullptr);

// Emits TEST_PASS / TEST_FAIL for checks the firmware can decide on its own.
void report_final(FinalStatus status, const char* detail = nullptr);

// Waits until everything written to the console has actually left the chip.
// Required before the firmware parks, sleeps or cuts power: the USB Serial/JTAG
// console otherwise loses the tail of the last line.
void console_flush();

// Blocks forever without touching any peripheral.
[[noreturn]] void hold_safe_idle();

const char* reset_reason_name(esp_reset_reason_t reason);
const char* wake_cause_name(esp_sleep_wakeup_cause_t cause);
const char* profile_name(Profile profile);
// Profile selected by CONFIG_M5CC_IMU_WAKE_PROFILE.
Profile configured_profile();

}  // namespace m5cc::test_support
