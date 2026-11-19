#include "m5cc_rtc.hpp"

#include <cstdint>
#include <ctime>

#include "driver/gpio.h"
#include "esp_log.h"
#include "m5cc_board.hpp"
#include "rx8130.h"

namespace m5cc::rtc {
namespace {

constexpr char kTag[] = "m5cc_rtc";

bool g_initialized = false;

Result from_esp(esp_err_t error)
{
    return {error, static_cast<int>(error)};
}

bool alarm_enabled(std::uint8_t control0)
{
    return (control0 & CTRL0_AIE) != 0;
}

bool timer_enabled(std::uint8_t control0, std::uint8_t extension)
{
    return (control0 & CTRL0_TIE) != 0 && (extension & EXT_TE) != 0;
}

}  // namespace

Result begin()
{
    if (g_initialized) {
        return {};
    }
    const esp_err_t init_result = rx8130_init(board::i2c_bus(), GPIO_NUM_NC);
    if (init_result != ESP_OK) {
        return from_esp(init_result);
    }
    g_initialized = true;
    return {};
}

bool initialized()
{
    return g_initialized;
}

Result disable_interrupts()
{
    if (!g_initialized) {
        return {ESP_ERR_INVALID_STATE, 0};
    }

    std::uint8_t control0 = 0;
    Result result         = from_esp(rx8130_read_control0(&control0));
    if (!result.ok()) {
        return result;
    }
    control0 &= static_cast<std::uint8_t>(~(CTRL0_AIE | CTRL0_TIE));
    result = from_esp(rx8130_write_control0(control0));
    if (!result.ok()) {
        return result;
    }

    std::uint8_t extension = 0;
    result                 = from_esp(rx8130_read_extension_register(&extension));
    if (!result.ok()) {
        return result;
    }
    extension &= static_cast<std::uint8_t>(~EXT_TE);
    result = from_esp(rx8130_write_extension_register(extension));
    if (!result.ok()) {
        return result;
    }

    result = from_esp(rx8130_read_control0(&control0));
    if (!result.ok()) {
        return result;
    }
    if ((control0 & (CTRL0_AIE | CTRL0_TIE)) != 0) {
        return {ESP_ERR_INVALID_RESPONSE, static_cast<int>(control0)};
    }
    result = from_esp(rx8130_read_extension_register(&extension));
    if (!result.ok()) {
        return result;
    }
    if ((extension & EXT_TE) != 0) {
        return {ESP_ERR_INVALID_RESPONSE, static_cast<int>(extension)};
    }
    return from_esp(rx8130_clear_all_flags());
}

Result arm_alarm(std::uint32_t seconds)
{
    if (!g_initialized || seconds == 0) {
        return {ESP_ERR_INVALID_STATE, 0};
    }

    // The alarm register only matches whole minutes, so place the reference time
    // such that the next minute boundary is exactly `seconds` away.
    const std::uint32_t offset   = seconds % 60U;
    const std::uint32_t base_sec = (60U - offset) % 60U;

    // RX8130 requires the oscillator to be stopped before the time registers are
    // written. Without CTRL0.STOP the write can be ignored and the alarm then
    // never matches, which made this test wake-less while the timer variant
    // (independent of time of day) still worked.
    std::uint8_t control0_before = 0;
    Result result                = from_esp(rx8130_read_control0(&control0_before));
    if (!result.ok()) {
        return result;
    }
    result = from_esp(rx8130_write_control0(static_cast<std::uint8_t>(control0_before | CTRL0_STOP)));
    if (!result.ok()) {
        return result;
    }

    struct tm reference = {};
    reference.tm_year   = 2026 - 1900;
    reference.tm_mon    = 0;
    reference.tm_mday   = 1;
    reference.tm_hour   = 0;
    reference.tm_min    = 0;
    reference.tm_sec    = static_cast<int>(base_sec);
    reference.tm_wday   = 4;  // 2026-01-01 is a Thursday.
    result              = from_esp(rx8130_set_time(&reference));
    if (!result.ok()) {
        return result;
    }

    // Restart the clock, keeping whatever control bits were already configured.
    result = from_esp(rx8130_write_control0(static_cast<std::uint8_t>(control0_before & ~CTRL0_STOP)));
    if (!result.ok()) {
        return result;
    }

    // Read the counters back: a silent write failure here is exactly what makes an
    // alarm test look like "armed" but never wake.
    struct tm written = {};
    if (rx8130_get_time(&written) == ESP_OK) {
        ESP_LOGI(kTag, "RTC_TIME base=%02d:%02d:%02d", written.tm_hour, written.tm_min, written.tm_sec);
    }

    // The alarm must sit `seconds` after the reference time, not just after the
    // next minute boundary: with seconds >= 60 the old offset-only form pointed at
    // the current minute and the alarm only fired an hour later.
    const std::uint32_t total = base_sec + seconds;
    const int hour            = static_cast<int>((total / 3600U) % 24U);
    const int minute          = static_cast<int>((total % 3600U) / 60U);
    // Weekday and day are disabled: the alarm repeats daily at this time.
    result = from_esp(rx8130_set_alarm(-1, -1, hour, minute));
    if (!result.ok()) {
        return result;
    }
    result = from_esp(rx8130_clear_alarm_flag());
    if (!result.ok()) {
        return result;
    }

    std::uint8_t control0 = 0;
    result                = from_esp(rx8130_read_control0(&control0));
    if (!result.ok()) {
        return result;
    }

    return alarm_enabled(control0) ? Result{} : Result{ESP_ERR_INVALID_RESPONSE, static_cast<int>(control0)};
}

Result arm_timer(std::uint32_t seconds)
{
    if (!g_initialized || seconds == 0) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    // Whole minutes use the 1/60 Hz clock for long counts; anything else uses
    // 1 Hz so that sub-minute arming (for example 30 s) stays exact.
    std::uint32_t counts          = 0;
    rx8130_timer_freq_t frequency = RX8130_TIMER_FREQ_1HZ;
    if ((seconds % 60U) == 0U) {
        frequency = RX8130_TIMER_FREQ_1_60HZ;
        counts    = seconds / 60U;
        if (counts > 255U) {
            counts = 255U;
        }
    } else {
        counts = seconds > 255U ? 255U : seconds;
    }

    Result result = from_esp(rx8130_start_timer(static_cast<std::uint8_t>(counts), frequency));
    if (!result.ok()) {
        return result;
    }
    result = from_esp(rx8130_clear_timer_flag());
    if (!result.ok()) {
        return result;
    }

    std::uint8_t control0  = 0;
    std::uint8_t extension = 0;
    result                 = from_esp(rx8130_read_control0(&control0));
    if (!result.ok()) {
        return result;
    }
    result = from_esp(rx8130_read_extension_register(&extension));
    if (!result.ok()) {
        return result;
    }
    return timer_enabled(control0, extension)
               ? Result{}
               : Result{ESP_ERR_INVALID_RESPONSE, static_cast<int>(control0) | (static_cast<int>(extension) << 8)};
}

Result read_wake_flags(WakeFlags* flags)
{
    if (!g_initialized || flags == nullptr) {
        return {ESP_ERR_INVALID_ARG, 0};
    }
    bool alarm    = false;
    bool timer    = false;
    Result result = from_esp(rx8130_get_alarm_flag(&alarm));
    if (!result.ok()) {
        return result;
    }
    result = from_esp(rx8130_get_timer_flag(&timer));
    if (!result.ok()) {
        return result;
    }
    flags->alarm = alarm;
    flags->timer = timer;
    return {};
}

Result clear_wake_flags()
{
    if (!g_initialized) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    return from_esp(rx8130_clear_all_flags());
}

}  // namespace m5cc::rtc
