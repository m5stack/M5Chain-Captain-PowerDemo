#pragma once

#include <cstdint>

#include "esp_err.h"

namespace m5cc::audio {

struct Result {
    esp_err_t error = ESP_OK;
    int raw_error   = 0;

    [[nodiscard]] bool ok() const
    {
        return error == ESP_OK;
    }
};

// Enables the codec rail and the PA, then initializes the ES8311 at 22050 Hz,
// 16 bit, stereo. Volume is applied through the codec, not the PA gain.
[[nodiscard]] Result start(std::uint8_t volume_percent);
bool started();

// Prepares the 1 kHz-class test tone buffer; call once after start().
[[nodiscard]] Result set_tone(std::uint32_t frequency_hz);

// Writes one buffer to the codec. Blocks until the I2S DMA accepts it, which
// paces the loop at roughly real time.
[[nodiscard]] Result pump();

// Mutes the codec, drops the PA and removes the codec rail.
[[nodiscard]] Result stop();

}  // namespace m5cc::audio
