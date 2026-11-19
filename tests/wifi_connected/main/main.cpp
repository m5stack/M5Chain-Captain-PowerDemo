#include <cstdint>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5cc_rtc.hpp"
#include "m5cc_test_support.hpp"
#include "m5cc_wifi.hpp"

namespace {

constexpr char kTag[]                     = "wifi_connected";
constexpr std::uint32_t kConnectTimeoutMs = 15000;
constexpr TickType_t kSettleDelay         = pdMS_TO_TICKS(3000);

[[noreturn]] void fail_board(const char* stage, esp_err_t error, int raw = 0)
{
    ESP_LOGE(kTag, "BOARD_FAIL stage=%s error=%s raw=%d", stage, esp_err_to_name(error), raw);
    m5cc::test_support::report_final(m5cc::test_support::FinalStatus::fail, stage);
    m5cc::test_support::hold_safe_idle();
}

[[noreturn]] void fail_state(const char* stage, esp_err_t error, int raw = 0)
{
    ESP_LOGE(kTag, "TEST_FAIL stage=%s error=%s raw=%d", stage, esp_err_to_name(error), raw);
    m5cc::test_support::report_final(m5cc::test_support::FinalStatus::fail, stage);
    m5cc::test_support::hold_safe_idle();
}

}  // namespace

extern "C" void app_main(void)
{
    if (m5cc::test_support::initialize() != ESP_OK) {
        return;
    }

    static m5cc::wifi::WifiController wifi;
    if (!wifi.credentials_configured()) {
        fail_state("wifi_credentials", ESP_ERR_INVALID_STATE);
    }

    const esp_err_t board_result = m5cc::board::initialize();
    if (board_result != ESP_OK) {
        fail_board("board_init", board_result);
    }

    m5cc::rtc::Result rtc_result = m5cc::rtc::begin();
    if (!rtc_result.ok()) {
        fail_board("rtc_begin", rtc_result.error, rtc_result.raw_error);
    }
    rtc_result = m5cc::rtc::disable_interrupts();
    if (!rtc_result.ok()) {
        fail_board("rtc_irq_disable", rtc_result.error, rtc_result.raw_error);
    }

    m5cc::wifi::WifiResult wifi_result = wifi.begin();
    if (!wifi_result.ok()) {
        fail_board("wifi_begin", wifi_result.error, wifi_result.raw_error);
    }
    wifi_result = wifi.set_power_save(m5cc::wifi::PowerSave::none);
    if (!wifi_result.ok()) {
        fail_board("wifi_ps_none", wifi_result.error, wifi_result.raw_error);
    }
    wifi_result = wifi.connect(kConnectTimeoutMs);
    if (!wifi_result.ok()) {
        fail_state("wifi_connect", wifi_result.error, wifi_result.raw_error);
    }
    if (wifi.state() != m5cc::wifi::State::connected || wifi.power_save() != m5cc::wifi::PowerSave::none) {
        fail_state("wifi_state", ESP_ERR_INVALID_RESPONSE);
    }

    m5cc::wifi::NetworkInfo info = {};
    wifi_result                  = wifi.network_info(&info);
    if (!wifi_result.ok()) {
        fail_state("wifi_info", wifi_result.error, wifi_result.raw_error);
    }
    if (info.channel == 0U || info.channel > 14U || info.ipv4_address == 0U) {
        fail_state("wifi_network", ESP_ERR_INVALID_RESPONSE);
    }

    ESP_LOGI(kTag, "BOARD_OK PM1=1 IOE=1 RTC=1 WIFI=connected PS=none CHANNEL=%u RSSI_DBM=%d TRAFFIC=0 DISPLAY=0",
             static_cast<unsigned>(info.channel), static_cast<int>(info.rssi_dbm));

    if (m5cc::test_support::mark_ready() != ESP_OK) {
        fail_state("test_ready", ESP_ERR_INVALID_STATE);
    }
    vTaskDelay(kSettleDelay);

    m5cc::test_support::state_entered("wifi_connected", "ps=none traffic=0 display=0");
    m5cc::test_support::hold_safe_idle();
}
