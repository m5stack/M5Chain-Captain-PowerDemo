#include <cstdint>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5cc_rtc.hpp"
#include "m5cc_test_support.hpp"
#include "m5cc_wifi.hpp"
#include "sdkconfig.h"

namespace {

constexpr char kTag[]                     = "wifi_light_sleep";
constexpr std::uint32_t kConnectTimeoutMs = 15000;
constexpr TickType_t kSettleDelay         = pdMS_TO_TICKS(3000);
constexpr int kCpuMinFrequencyMhz         = 80;
constexpr int kCpuMaxFrequencyMhz         = 240;

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

void configure_power_management()
{
    esp_pm_config_t requested    = {};
    requested.max_freq_mhz       = kCpuMaxFrequencyMhz;
    requested.min_freq_mhz       = kCpuMinFrequencyMhz;
    requested.light_sleep_enable = true;
    esp_err_t result             = esp_pm_configure(&requested);
    if (result != ESP_OK) {
        fail_state("pm_configure", result);
    }

    esp_pm_config_t actual = {};
    result                 = esp_pm_get_configuration(&actual);
    if (result != ESP_OK) {
        fail_state("pm_readback", result);
    }
    if (actual.max_freq_mhz != requested.max_freq_mhz || actual.min_freq_mhz != requested.min_freq_mhz ||
        actual.light_sleep_enable != requested.light_sleep_enable) {
        fail_state("pm_verify", ESP_ERR_INVALID_RESPONSE);
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
    wifi_result = wifi.connect(kConnectTimeoutMs);
    if (!wifi_result.ok()) {
        fail_state("wifi_connect", wifi_result.error, wifi_result.raw_error);
    }
    if (wifi.state() != m5cc::wifi::State::connected) {
        fail_state("wifi_state", ESP_ERR_INVALID_RESPONSE);
    }

    wifi_result = wifi.set_power_save(m5cc::wifi::PowerSave::max_modem);
    if (!wifi_result.ok()) {
        fail_state("wifi_ps_max_modem", wifi_result.error, wifi_result.raw_error);
    }
    if (wifi.state() != m5cc::wifi::State::connected || wifi.power_save() != m5cc::wifi::PowerSave::max_modem) {
        fail_state("wifi_power_state", ESP_ERR_INVALID_RESPONSE);
    }

    m5cc::wifi::NetworkInfo info = {};
    wifi_result                  = wifi.network_info(&info);
    if (!wifi_result.ok()) {
        fail_state("wifi_info", wifi_result.error, wifi_result.raw_error);
    }
    if (info.listen_interval != CONFIG_M5CC_WIFI_LISTEN_INTERVAL || info.channel == 0U || info.channel > 14U ||
        info.ipv4_address == 0U) {
        fail_state("wifi_network", ESP_ERR_INVALID_RESPONSE);
    }

    ESP_LOGI(kTag,
             "BOARD_OK PM1=1 IOE=1 RTC=1 WIFI=connected PS=max_modem LISTEN_INTERVAL=%u CPU_MIN_MHZ=%d "
             "CPU_MAX_MHZ=%d LIGHT_SLEEP=1 CHANNEL=%u RSSI_DBM=%d TRAFFIC=0 DISPLAY=0",
             static_cast<unsigned>(info.listen_interval), kCpuMinFrequencyMhz, kCpuMaxFrequencyMhz,
             static_cast<unsigned>(info.channel), static_cast<int>(info.rssi_dbm));

    if (m5cc::test_support::mark_ready() != ESP_OK) {
        fail_state("test_ready", ESP_ERR_INVALID_STATE);
    }
    vTaskDelay(kSettleDelay);

    m5cc::test_support::state_entered("wifi_light_sleep", "ps=max_modem light_sleep=1 traffic=0 display=0");

    // Automatic light sleep gates the USB Serial/JTAG console, so it is enabled
    // only after every line of evidence has left the chip; otherwise the run
    // reaches the state but STATE_ENTERED never appears on the host.
    m5cc::test_support::console_flush();
    configure_power_management();

    m5cc::test_support::hold_safe_idle();
}
