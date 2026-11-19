#pragma once

#include <cstdint>

#include "esp_err.h"

namespace m5cc::display {

struct Result {
    esp_err_t error = ESP_OK;
    int raw_error   = 0;

    [[nodiscard]] bool ok() const
    {
        return error == ESP_OK;
    }
};

// Powers the TFT rail, initializes the JD9853 panel and starts the LVGL port.
// The backlight stays off until set_brightness() is called.
[[nodiscard]] Result start();
bool started();

[[nodiscard]] Result set_brightness(std::uint8_t percent);

// Static information frame. ASCII only: this build ships no CJK font.
[[nodiscard]] Result show_frame(const char* title, const char* line1, const char* line2);

// Fills the panel with a flat white image (every pixel on, no text). Used by the
// full-load test so the LCD contributes its maximum current.
[[nodiscard]] Result show_solid_white();

// Updates the seconds counter and invalidates the whole screen, so one full
// frame is flushed per call.
[[nodiscard]] Result tick_1hz(std::uint32_t seconds);

// Starts a background task that invalidates the whole screen at the given rate.
// Used by the full-load test to produce a continuous display workload.
[[nodiscard]] Result start_continuous_draw(std::uint32_t hz);
bool continuous_draw_running();

}  // namespace m5cc::display
