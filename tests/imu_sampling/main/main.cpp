#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5cc_imu.hpp"
#include "m5cc_rtc.hpp"
#include "m5cc_test_support.hpp"

namespace {

constexpr char kTag[]             = "imu_sampling";
constexpr TickType_t kSettleDelay = pdMS_TO_TICKS(3000);
constexpr std::uint32_t kSampleHz = 100;
constexpr TickType_t kProbeDelay  = pdMS_TO_TICKS(500);

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

    // Active load: accelerometer + gyroscope at 100 Hz with the advanced
    // power-save mode off, sampled from a background task. Display, audio, RGB
    // and every L3B rail stay off, so this is an isolated IMU workload.
    const m5cc::imu::Result sampling_result = m5cc::imu::start_sampling(kSampleHz);
    if (!sampling_result.ok()) {
        fail_state("imu_sampling", sampling_result.error);
    }
    vTaskDelay(kProbeDelay);
    ESP_LOGI(kTag, "IMU_SAMPLING hz=%u samples=%u", static_cast<unsigned>(kSampleHz),
             static_cast<unsigned>(m5cc::imu::sample_count()));

    m5cc::test_support::state_entered("imu_sampling", "accel_hz=100 gyro_hz=100 display=off");
    m5cc::test_support::hold_safe_idle();
}
