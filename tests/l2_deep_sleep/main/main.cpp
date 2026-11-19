#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5cc_imu.hpp"
#include "m5cc_power.hpp"
#include "m5cc_rtc.hpp"
#include "m5cc_test_support.hpp"

namespace {

constexpr char kTag[]                      = "l2_deep_sleep";
constexpr TickType_t kSettleDelay          = pdMS_TO_TICKS(3000);
constexpr std::uint32_t kDeepSleepSettleMs = 1000;

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

}  // namespace

extern "C" void app_main(void)
{
    if (m5cc::test_support::initialize() != ESP_OK) {
        return;
    }

    const esp_err_t board_result = m5cc::board::initialize_for_l2();
    if (board_result != ESP_OK) {
        fail_board("board_init", board_result);
    }

    if (m5cc::test_support::mark_ready() != ESP_OK) {
        fail_state("test_ready", ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE);
    }
    vTaskDelay(kSettleDelay);

    // The standby entry owns every rail change and does not return.
    m5cc::test_support::state_entered("l2_deep_sleep");
    m5cc::board::enter_l2_standby();
}
