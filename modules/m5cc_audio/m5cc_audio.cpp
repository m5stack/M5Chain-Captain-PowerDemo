#include "m5cc_audio.hpp"

#include <cmath>
#include <cstddef>

#include "esp_log.h"
#include "driver/gpio.h"
#include "m5cc_board.hpp"
#include "m5stack_chain_captain.h"

namespace m5cc::audio {
namespace {

constexpr char kTag[]               = "m5cc_audio";
constexpr std::uint32_t kSampleRate = 22050;
constexpr std::size_t kFrames       = 512;  // ~23 ms of mono audio per write
constexpr float kTwoPi              = 6.283185307179586f;
constexpr float kAmplitude          = 26000.0f;  // about -2 dBFS

bool g_started                 = false;
std::int16_t g_buffer[kFrames] = {};
float g_phase                  = 0.0f;
float g_phase_step             = 0.0f;

}  // namespace

Result start(std::uint8_t volume_percent)
{
    if (g_started) {
        return {};
    }
    esp_err_t rail_result = board::set_rail(board::Rail::codec, true);
    if (rail_result != ESP_OK) {
        return {rail_result, 0};
    }
    const esp_err_t init_result = bsp_extra_codec_init();
    if (init_result != ESP_OK) {
        board::set_rail(board::Rail::codec, false);
        return {init_result, 0};
    }
    // The validated factory path uses MONO at 22050 Hz; opening the codec in
    // stereo made the same buffer play back at the wrong rate and level.
    const esp_err_t fs_result = bsp_extra_codec_set_fs(kSampleRate, 16, I2S_SLOT_MODE_MONO);
    if (fs_result != ESP_OK) {
        return {fs_result, 0};
    }
    if (bsp_extra_codec_mute_set(true) != ESP_OK) {
        return {ESP_FAIL, 0};
    }
    // The gain-mode pulse train leaves the PA pin low; hold it enabled for the
    // whole playback session instead of relying on the codec driver.
    gpio_set_level(board::kPaEnable, 1);
    int volume_set = 0;
    if (bsp_extra_codec_volume_set(volume_percent, &volume_set) != ESP_OK) {
        return {ESP_FAIL, 0};
    }
    if (bsp_extra_codec_mute_set(false) != ESP_OK) {
        return {ESP_FAIL, 0};
    }
    g_started = true;
    ESP_LOGI(kTag, "AUDIO_STARTED mono=%uHz volume=%d", static_cast<unsigned>(kSampleRate), volume_set);
    return {};
}

bool started()
{
    return g_started;
}

Result set_tone(std::uint32_t frequency_hz)
{
    if (!g_started || frequency_hz == 0) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    g_phase_step = kTwoPi * static_cast<float>(frequency_hz) / static_cast<float>(kSampleRate);
    g_phase      = 0.0f;
    return {};
}

Result pump()
{
    if (!g_started) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    for (std::size_t index = 0; index < kFrames; ++index) {
        const auto sample = static_cast<std::int16_t>(std::sin(g_phase) * kAmplitude);
        g_phase += g_phase_step;
        if (g_phase >= kTwoPi) {
            g_phase -= kTwoPi;
        }
        g_buffer[index] = sample;
    }
    std::size_t written = 0;
    const esp_err_t err = bsp_extra_i2s_write(g_buffer, sizeof(g_buffer), &written, 1000);
    if (err != ESP_OK) {
        return {err, 0};
    }
    return written == sizeof(g_buffer) ? Result{} : Result{ESP_ERR_INVALID_SIZE, 0};
}

Result stop()
{
    if (!g_started) {
        return {};
    }
    bsp_extra_codec_mute_set(true);
    const esp_err_t rail_result = board::set_rail(board::Rail::codec, false);
    board::configure_pa_off();
    g_started = false;
    return {rail_result, 0};
}

}  // namespace m5cc::audio
