#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5cc_power.hpp"
#include "m5cc_test_support.hpp"

namespace {

constexpr char kTag[]             = "l0_shutdown";
constexpr TickType_t kSettleDelay = pdMS_TO_TICKS(3000);

// Diagnostic build: keep the charger enabled in L0 to measure its contribution.
#ifdef CONFIG_M5CC_L0_KEEP_CHARGE_EN
constexpr bool kKeepChargeEnabled = true;
#else
constexpr bool kKeepChargeEnabled = false;
#endif

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

    const esp_err_t board_result = m5cc::board::initialize();
    if (board_result != ESP_OK) {
        fail_board("board_init", board_result);
    }
    ESP_LOGI(kTag, "BOARD_OK PM1=1 IOE=1");

    if (m5cc::test_support::mark_ready() != ESP_OK) {
        fail_state("test_ready", ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE);
    }
    vTaskDelay(kSettleDelay);

    m5cc::test_support::state_entered("l0_shutdown", "ldo=off");
    const m5cc::power::Result l0_result = m5cc::power::enter_l0(kKeepChargeEnabled);
    if (!l0_result.ok()) {
        fail_state("l0_enter", l0_result.error, l0_result.raw_error);
    }

    // Reaching this point means the PMIC accepted the shutdown command but the
    // board is still powered. Stay idle instead of touching any peripheral.
    m5cc::test_support::hold_safe_idle();
}
