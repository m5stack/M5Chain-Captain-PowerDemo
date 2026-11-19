#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5cc_display.hpp"
#include "m5cc_imu.hpp"
#include "m5cc_rtc.hpp"
#include "m5cc_test_support.hpp"

namespace {

constexpr char kTag[]               = "normal_active";
constexpr TickType_t kSettleDelay   = pdMS_TO_TICKS(3000);
constexpr TickType_t kRefreshPeriod = pdMS_TO_TICKS(1000);
constexpr std::uint8_t kBrightness  = 50;

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

    m5cc::imu::Result imu_result = m5cc::imu::begin();
    if (!imu_result.ok()) {
        fail_board("imu_begin", imu_result.error);
    }
    imu_result = m5cc::imu::suspend();
    if (!imu_result.ok()) {
        fail_board("imu_suspend", imu_result.error);
    }

    ESP_LOGI(kTag, "BOARD_OK PM1=1 IOE=1 RTC=1 IMU=1");

    if (m5cc::test_support::mark_ready() != ESP_OK) {
        fail_state("test_ready", ESP_ERR_INVALID_STATE);
    }
    vTaskDelay(kSettleDelay);

    // L3B entry: the display stack is initialized only after the USB cable has
    // been removed, so panel setup does not overlap the settle window.
    m5cc::display::Result display_result = m5cc::display::start();
    if (!display_result.ok()) {
        fail_state("display_start", display_result.error);
    }
    display_result = m5cc::display::set_brightness(kBrightness);
    if (!display_result.ok()) {
        fail_state("backlight", display_result.error);
    }
    display_result = m5cc::display::show_frame("normal-active", "LCD 50% / 1 Hz", "IMU+RTC idle");
    if (!display_result.ok()) {
        fail_state("show_frame", display_result.error);
    }

    m5cc::test_support::state_entered("normal_active", "brightness=50 refresh_hz=1");

    std::uint32_t seconds = 0;
    for (;;) {
        vTaskDelay(kRefreshPeriod);
        ++seconds;
        const m5cc::display::Result tick_result = m5cc::display::tick_1hz(seconds);
        if (!tick_result.ok()) {
            fail_state("tick_1hz", tick_result.error);
        }
    }
}
