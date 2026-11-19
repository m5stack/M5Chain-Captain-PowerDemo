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

constexpr char kTag[]                     = "wifi_periodic_traffic";
constexpr std::uint32_t kConnectTimeoutMs = 15000;
constexpr TickType_t kSettleDelay         = pdMS_TO_TICKS(3000);
constexpr std::uint32_t kPingIntervalMs   = 1000;
constexpr std::uint32_t kPingTimeoutMs    = 900;

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

// One 64-byte ICMP echo per second. The payload size and the interval are fixed by
// the power plan so the measured traffic stays reproducible across runs.
void traffic_task(void* arg)
{
    auto* wifi = static_cast<m5cc::wifi::WifiController*>(arg);
    for (;;) {
        const m5cc::wifi::WifiResult result = wifi->ping_once(kPingTimeoutMs);
        if (!result.ok()) {
            ESP_LOGW(kTag, "PING_SKIP error=%s raw=%d", esp_err_to_name(result.error), result.raw_error);
        }
        vTaskDelay(pdMS_TO_TICKS(kPingIntervalMs));
    }
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
    if (!wifi.target_configured()) {
        fail_state("wifi_target", ESP_ERR_INVALID_STATE);
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

    ESP_LOGI(kTag, "BOARD_OK PM1=1 IOE=1 RTC=1 WIFI=connected PS=none CHANNEL=%u RSSI_DBM=%d PING_MS=%u PAYLOAD=64",
             static_cast<unsigned>(info.channel), static_cast<int>(info.rssi_dbm),
             static_cast<unsigned>(kPingIntervalMs));

    if (m5cc::test_support::mark_ready() != ESP_OK) {
        fail_state("test_ready", ESP_ERR_INVALID_STATE);
    }
    vTaskDelay(kSettleDelay);

    if (xTaskCreate(traffic_task, "wifi_traffic", 4096, &wifi, 4, nullptr) != pdPASS) {
        fail_state("traffic_task", ESP_ERR_NO_MEM);
    }

    m5cc::test_support::state_entered("wifi_periodic_traffic", "ps=none ping_1hz=64B display=0");
    m5cc::test_support::hold_safe_idle();
}
