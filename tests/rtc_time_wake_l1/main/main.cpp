#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5cc_power.hpp"
#include "m5cc_rtc.hpp"
#include "m5cc_test_support.hpp"

namespace {

constexpr char kTag[]                     = "rtc_time_wake_l1";
constexpr TickType_t kSettleDelay         = pdMS_TO_TICKS(3000);
constexpr std::uint32_t kWakeAfterSeconds = 120;

[[noreturn]] void fail_board(const char* stage, esp_err_t error)
{
    ESP_LOGE(kTag, "BOARD_FAIL stage=%s error=%s", stage, esp_err_to_name(error));
    m5cc::test_support::report_final(m5cc::test_support::FinalStatus::fail, stage);
    m5cc::test_support::hold_safe_idle();
}

[[noreturn]] void fail_state(const char* stage, esp_err_t error, int raw_error)
{
    ESP_LOGE(kTag, "TEST_FAIL stage=%s error=%s raw=%d", stage, esp_err_to_name(error), raw_error);
    m5cc::test_support::report_final(m5cc::test_support::FinalStatus::fail, stage);
    m5cc::test_support::hold_safe_idle();
}

void log_wake_evidence()
{
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    std::uint8_t wake_source             = 0;
    std::uint8_t gpio_irq                = 0;
    if (!m5cc::power::read_pmic_wake(&wake_source, &gpio_irq).ok()) {
        return;
    }

    m5cc::rtc::WakeFlags flags = {};
    const bool flags_ok        = m5cc::rtc::read_wake_flags(&flags).ok();
    if (wake_source == 0 && gpio_irq == 0 && cause == ESP_SLEEP_WAKEUP_UNDEFINED && !flags.alarm && !flags.timer) {
        return;
    }

    ESP_LOGI(kTag, "PMIC_WAKE src=0x%02X gpio_irq=0x%02X rtc_alarm=%d", wake_source, gpio_irq, flags.alarm ? 1 : 0);
    if (flags_ok) {
        ESP_LOGI(kTag, "RTC_WAKE alarm=%d timer=%d", flags.alarm ? 1 : 0, flags.timer ? 1 : 0);
    }
    m5cc::power::clear_pmic_wake();
    m5cc::rtc::clear_wake_flags();
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
    log_wake_evidence();

    // This project is the timer variant; the calendar-alarm variant lives in
    // tests/rtc_alarm_wake_l1.
    rtc_result = m5cc::rtc::arm_timer(kWakeAfterSeconds);
    if (!rtc_result.ok()) {
        fail_state("rtc_arm", rtc_result.error, rtc_result.raw_error);
    }

    ESP_LOGI(kTag, "RTC_ALARM armed=+%us mode=timer", static_cast<unsigned>(kWakeAfterSeconds));
    ESP_LOGI(kTag, "BOARD_OK PM1=1 IOE=1 RTC=1");

    if (m5cc::test_support::mark_ready() != ESP_OK) {
        fail_state("test_ready", ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE);
    }
    vTaskDelay(kSettleDelay);

    m5cc::test_support::state_entered("rtc_time_wake_l1", "wake=pmic_g2 timer=120s ldo_hold=1");
    const m5cc::power::Result l1_result = m5cc::power::enter_l1(m5cc::power::WakeSource::rtc);
    if (!l1_result.ok()) {
        fail_state("l1_enter", l1_result.error, l1_result.raw_error);
    }

    m5cc::test_support::hold_safe_idle();
}
