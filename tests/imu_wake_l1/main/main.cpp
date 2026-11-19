#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5cc_imu.hpp"
#include "m5cc_power.hpp"
#include "m5cc_test_support.hpp"

namespace {

constexpr char kTag[]             = "imu_wake_l1";
constexpr TickType_t kSettleDelay = pdMS_TO_TICKS(3000);

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
    if (wake_source == 0 && gpio_irq == 0 && cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
        return;  // Cold boot: nothing to report.
    }
    ESP_LOGI(kTag, "PMIC_WAKE src=0x%02X gpio_irq=0x%02X", wake_source, gpio_irq);
    m5cc::power::clear_pmic_wake();
}

}  // namespace

extern "C" void app_main(void)
{
    const m5cc::test_support::Profile profile = m5cc::test_support::configured_profile();
    if (m5cc::test_support::initialize(profile) != ESP_OK) {
        return;
    }

    const esp_err_t board_result = m5cc::board::initialize();
    if (board_result != ESP_OK) {
        fail_board("board_init", board_result);
    }
    log_wake_evidence();

    m5cc::imu::Result imu_result = m5cc::imu::begin();
    if (!imu_result.ok()) {
        fail_board("imu_begin", imu_result.error);
    }

    const m5cc::imu::WakeProfile imu_profile = profile == m5cc::test_support::Profile::reference
                                                   ? m5cc::imu::WakeProfile::reference
                                                   : m5cc::imu::WakeProfile::low_power;
    imu_result                               = m5cc::imu::configure_any_motion(imu_profile);
    if (!imu_result.ok()) {
        fail_board("imu_any_motion", imu_result.error);
    }
    // Drop any event captured during setup so the PMIC input starts low.
    imu_result = m5cc::imu::clear_wake_status();
    if (!imu_result.ok()) {
        fail_board("imu_status_clear", imu_result.error);
    }

    const bool reference = imu_profile == m5cc::imu::WakeProfile::reference;
    ESP_LOGI(kTag, "PROFILE profile=%s accel=%s gyro=%s feature=any_motion", m5cc::test_support::profile_name(profile),
             reference ? "100Hz" : m5cc::imu::accel_odr_label(), reference ? "100Hz" : "off");
    ESP_LOGI(kTag, "BOARD_OK PM1=1 IOE=1 IMU=1");

    if (m5cc::test_support::mark_ready() != ESP_OK) {
        fail_state("test_ready", ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE);
    }
    vTaskDelay(kSettleDelay);

    // PMIC stays powered but the 3V3_L1 rail is held for the IMU; a motion event
    // on PMIC G4 brings the whole board back up.
    m5cc::test_support::state_entered("imu_wake_l1", "wake=pmic_g4 ldo_hold=1");
    const m5cc::power::Result l1_result = m5cc::power::enter_l1(m5cc::power::WakeSource::imu);
    if (!l1_result.ok()) {
        fail_state("l1_enter", l1_result.error, l1_result.raw_error);
    }

    m5cc::test_support::hold_safe_idle();
}
