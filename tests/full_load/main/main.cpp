#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_audio.hpp"
#include "m5cc_board.hpp"
#include "m5cc_display.hpp"
#include "m5cc_imu.hpp"
#include "m5cc_rtc.hpp"
#include "m5cc_test_support.hpp"

namespace {

constexpr char kTag[]                    = "full_load";
constexpr TickType_t kSettleDelay        = pdMS_TO_TICKS(3000);
constexpr std::uint8_t kBrightness       = 100;
constexpr std::uint8_t kVolumePercent    = 100;
constexpr std::uint32_t kToneHz          = 1000;
constexpr std::uint32_t kDrawHz          = 30;
constexpr std::uint32_t kSamplingHz      = 100;
constexpr std::uint32_t kMaxLoadSeconds  = CONFIG_M5CC_FULL_LOAD_MAX_SECONDS;
constexpr std::uint32_t kDegradedDelayMs = 50;

// Manual stop button: any of K1/K2/K3 ends the load immediately. The keys are
// wired straight to the ESP32 and idle high, so a low level means "pressed".
constexpr gpio_num_t kStopKeys[] = {m5cc::board::kKey1, m5cc::board::kKey2, m5cc::board::kKey3};

esp_err_t configure_stop_keys()
{
    gpio_config_t config = {};
    config.pin_bit_mask  = (1ULL << m5cc::board::kKey1) | (1ULL << m5cc::board::kKey2) | (1ULL << m5cc::board::kKey3);
    config.mode          = GPIO_MODE_INPUT;
    config.pull_up_en    = GPIO_PULLUP_ENABLE;
    config.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    config.intr_type     = GPIO_INTR_DISABLE;
    return gpio_config(&config);
}

bool stop_key_pressed()
{
    for (const gpio_num_t key : kStopKeys) {
        if (gpio_get_level(key) == 0) {
            return true;
        }
    }
    return false;
}

// The audio stream runs in its own high-priority task: a 30 Hz full-white refresh
// is heavy enough to starve the main loop, which used to silence the speaker. The
// I2S write blocks for one buffer, so the task still yields plenty of CPU.
TaskHandle_t g_audio_task     = nullptr;
volatile bool g_audio_running = false;

void audio_task(void*)
{
    std::uint32_t pumps = 0;
    ESP_LOGI(kTag, "AUDIO_TASK start");
    while (g_audio_running) {
        const m5cc::audio::Result result = m5cc::audio::pump();
        if (!result.ok()) {
            ESP_LOGE(kTag, "AUDIO_PUMP failed error=%d raw=%d pumps=%u", static_cast<int>(result.error),
                     result.raw_error, static_cast<unsigned>(pumps));
            break;
        }
        ++pumps;
        if ((pumps % 40U) == 0U) {
            ESP_LOGI(kTag, "AUDIO_OK pumps=%u", static_cast<unsigned>(pumps));
        }
    }
    ESP_LOGW(kTag, "AUDIO_TASK exit pumps=%u", static_cast<unsigned>(pumps));
    g_audio_task = nullptr;
    vTaskDelete(nullptr);
}

#ifdef CONFIG_M5CC_FULL_LOAD_SAFETY_ACK
constexpr bool kSafetyAcked = true;
#else
constexpr bool kSafetyAcked = false;
#endif

[[noreturn]] void fail_board(const char* stage, esp_err_t error)
{
    ESP_LOGE(kTag, "BOARD_FAIL stage=%s error=%s", stage, esp_err_to_name(error));
    m5cc::test_support::report_final(m5cc::test_support::FinalStatus::fail, stage);
    m5cc::test_support::hold_safe_idle();
}

[[noreturn]] void fail_state(const char* stage, esp_err_t error)
{
    ESP_LOGE(kTag, "TEST_FAIL stage=%s error=%s", stage, esp_err_to_name(error));
    m5cc::test_support::report_final(m5cc::test_support::FinalStatus::fail, stage);
    m5cc::test_support::hold_safe_idle();
}

}  // namespace

extern "C" void app_main(void)
{
    if (m5cc::test_support::initialize() != ESP_OK) {
        return;
    }

    const esp_err_t board_result = m5cc::board::initialize();
    if (board_result != ESP_OK) {
        fail_board("board_init", board_result);
    }

    m5cc::rtc::Result rtc_result = m5cc::rtc::begin();
    if (!rtc_result.ok()) {
        fail_board("rtc_begin", rtc_result.error);
    }
    rtc_result = m5cc::rtc::disable_interrupts();
    if (!rtc_result.ok()) {
        fail_board("rtc_irq_disable", rtc_result.error);
    }

    const m5cc::imu::Result imu_result = m5cc::imu::begin();
    if (!imu_result.ok()) {
        fail_board("imu_begin", imu_result.error);
    }

    ESP_LOGI(kTag, "BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1");

    if (m5cc::test_support::mark_ready() != ESP_OK) {
        fail_state("test_ready", ESP_ERR_INVALID_STATE);
    }
    vTaskDelay(kSettleDelay);

    if (!kSafetyAcked) {
        // Refuse to load audio, RGB and continuous refresh without a reviewed
        // fixture current limit, thermal limit and stop procedure.
        ESP_LOGW(kTag, "SAFETY_BLOCK reason=full_load_not_acked");
        m5cc::test_support::state_entered("l3a_idle", "safety=blocked");
        m5cc::test_support::hold_safe_idle();
    }

    m5cc::display::Result display_result = m5cc::display::start();
    if (!display_result.ok()) {
        fail_state("display_start", display_result.error);
    }
    display_result = m5cc::display::set_brightness(kBrightness);
    if (!display_result.ok()) {
        fail_state("backlight", display_result.error);
    }
    // Flat white panel: maximum LCD current, no text pixels.
    display_result = m5cc::display::show_solid_white();
    if (!display_result.ok()) {
        fail_state("show_white", display_result.error);
    }

    m5cc::audio::Result audio_result = m5cc::audio::start(kVolumePercent);
    if (!audio_result.ok()) {
        fail_state("audio_start", audio_result.error);
    }
    audio_result = m5cc::audio::set_tone(kToneHz);
    if (!audio_result.ok()) {
        fail_state("audio_tone", audio_result.error);
    }

    const esp_err_t rgb_result = m5cc::board::set_rgb(70, 100, 100);
    if (rgb_result != ESP_OK) {
        fail_state("rgb", rgb_result);
    }

    const m5cc::imu::Result sampling_result = m5cc::imu::start_sampling(kSamplingHz);
    if (!sampling_result.ok()) {
        fail_state("imu_sampling", sampling_result.error);
    }

    display_result = m5cc::display::start_continuous_draw(kDrawHz);
    if (!display_result.ok()) {
        fail_state("display_draw", display_result.error);
    }

    ESP_LOGI(kTag, "LOAD lcd=1 audio=1 rgb=1 imu=1 brightness=%u volume=%u tone_hz=%u draw_hz=%u",
             static_cast<unsigned>(kBrightness), static_cast<unsigned>(kVolumePercent), static_cast<unsigned>(kToneHz),
             static_cast<unsigned>(kDrawHz));
    m5cc::test_support::state_entered("full_load");
    ESP_LOGI(kTag, "STOP_HINT press_any_key=stop max_load=%us", static_cast<unsigned>(kMaxLoadSeconds));

    const esp_err_t key_result = configure_stop_keys();
    if (key_result != ESP_OK) {
        fail_state("stop_keys", key_result);
    }

    g_audio_running = true;
    if (xTaskCreate(audio_task, "m5cc_audio", 4096, nullptr, 5, &g_audio_task) != pdPASS) {
        g_audio_running = false;
        fail_state("audio_task", ESP_ERR_NO_MEM);
    }

    const std::int64_t start_us = esp_timer_get_time();
    bool degraded               = false;
    for (;;) {
        if (!degraded && stop_key_pressed()) {
            degraded        = true;
            g_audio_running = false;
            vTaskDelay(pdMS_TO_TICKS(120));
            m5cc::audio::stop();
            m5cc::display::set_brightness(50);
            ESP_LOGW(kTag, "STATE_EXIT reason=key imu_samples=%u", static_cast<unsigned>(m5cc::imu::sample_count()));
        }
        if (!degraded) {
            const std::int64_t elapsed_s = (esp_timer_get_time() - start_us) / 1000000;
            if (elapsed_s >= static_cast<std::int64_t>(kMaxLoadSeconds)) {
                degraded        = true;
                g_audio_running = false;
                vTaskDelay(pdMS_TO_TICKS(120));
                m5cc::audio::stop();
                m5cc::display::set_brightness(50);
                ESP_LOGW(kTag, "STATE_EXIT reason=timeout elapsed=%us imu_samples=%u", static_cast<unsigned>(elapsed_s),
                         static_cast<unsigned>(m5cc::imu::sample_count()));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(kDegradedDelayMs));
        }
    }
}
