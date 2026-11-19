#include <cinttypes>
#include <cstdint>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5cc_imu.hpp"
#include "m5cc_power.hpp"
#include "m5cc_rtc.hpp"
#include "m5cc_test_support.hpp"

namespace {

constexpr char kTag[]                      = "key_wake_l2";
constexpr TickType_t kSettleDelay          = pdMS_TO_TICKS(3000);
constexpr std::uint32_t kDeepSleepSettleMs = 1000;
constexpr int kReleasePollMs               = 50;
constexpr int kReleaseTimeoutMs            = 10000;

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

bool keys_released()
{
    // Keys are active low: released means the line sits at the pull-up level.
    return gpio_get_level(m5cc::board::kKey1) != 0 && gpio_get_level(m5cc::board::kKey2) != 0 &&
           gpio_get_level(m5cc::board::kKey3) != 0;
}

esp_err_t configure_key_inputs()
{
    gpio_config_t config = {};
    config.pin_bit_mask  = (1ULL << static_cast<std::uint32_t>(m5cc::board::kKey1)) |
                          (1ULL << static_cast<std::uint32_t>(m5cc::board::kKey2)) |
                          (1ULL << static_cast<std::uint32_t>(m5cc::board::kKey3));
    config.mode         = GPIO_MODE_INPUT;
    config.pull_up_en   = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type    = GPIO_INTR_DISABLE;
    return gpio_config(&config);
}

void log_wake_evidence()
{
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    const std::uint64_t ext1_mask        = esp_sleep_get_ext1_wakeup_status();

    std::uint8_t wake_source = 0;
    std::uint8_t gpio_irq    = 0;
    const bool pmic_ok       = m5cc::power::read_pmic_wake(&wake_source, &gpio_irq).ok();
    const bool pmic_clean    = !pmic_ok || (wake_source == 0 && gpio_irq == 0);

    if (cause == ESP_SLEEP_WAKEUP_UNDEFINED && ext1_mask == 0 && pmic_clean) {
        return;  // Cold boot: nothing to report.
    }

    ESP_LOGI(kTag, "WAKE cause=%s ext1_mask=0x%016" PRIx64, m5cc::test_support::wake_cause_name(cause), ext1_mask);
    if (pmic_ok) {
        ESP_LOGI(kTag, "PMIC_WAKE src=0x%02X gpio_irq=0x%02X", wake_source, gpio_irq);
    }
    m5cc::power::clear_pmic_wake();
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
    const esp_err_t key_result = configure_key_inputs();
    if (key_result != ESP_OK) {
        fail_board("key_inputs", key_result);
    }
    log_wake_evidence();

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

    // A key still held from the wake event would make EXT1 fire again as soon as
    // the chip sleeps. Wait (bounded) for every key to be released first.
    int waited_ms = 0;
    while (!keys_released() && waited_ms < kReleaseTimeoutMs) {
        vTaskDelay(pdMS_TO_TICKS(kReleasePollMs));
        waited_ms += kReleasePollMs;
    }
    if (!keys_released()) {
        ESP_LOGW(kTag, "KEYS_HELD waited=%dms", waited_ms);
    }

    // L2 with the three user keys as the only wake source. The PMIC stays in its
    // safe state, so this measurement is comparable with the other L2 builds.
    const m5cc::power::Result prepare_result = m5cc::power::prepare_l2(m5cc::power::WakeSource::key);
    if (!prepare_result.ok()) {
        fail_state("l2_prepare", prepare_result.error);
    }
    const m5cc::power::Result finalize_result = m5cc::power::finalize_l2();
    if (!finalize_result.ok()) {
        fail_state("l2_finalize", finalize_result.error);
    }

    m5cc::test_support::state_entered("key_wake_l2", "wake=ext1_k1k2k3 io_sleep=1");
    m5cc::power::enter_deep_sleep(kDeepSleepSettleMs);
}
