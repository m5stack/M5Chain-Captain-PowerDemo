#include "m5cc_test_support.hpp"

#include <cinttypes>
#include <cstdio>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace m5cc::test_support {
namespace {

constexpr char kTag[] = "m5cc_test";

// The USB Serial/JTAG console loses the tail of the last line when the CPU stops
// writing immediately afterwards. Giving the peripheral a few milliseconds to
// push the pending packet out is enough; blocking driver APIs are not available
// because the console runs on the VFS path, not on the USB-JTAG driver.
constexpr TickType_t kDrainSettle = pdMS_TO_TICKS(50);

bool g_initialized = false;
bool g_ready       = false;

std::uint32_t fnv1a(const char* text)
{
    std::uint32_t hash = 2166136261u;
    while (*text != '\0') {
        hash ^= static_cast<std::uint8_t>(*text++);
        hash *= 16777619u;
    }
    return hash;
}

}  // namespace

void console_flush()
{
    std::fflush(stdout);
    vTaskDelay(kDrainSettle);
    // The USB Serial/JTAG console only releases a pending chunk when another
    // write follows, and the last write before the CPU parks can be stranded.
    // Emit a disposable trailer so the real line is pushed out; the trailer is
    // what gets lost, not the evidence.
    std::fputc('\n', stdout);
    std::fflush(stdout);
    vTaskDelay(kDrainSettle);
}

esp_err_t initialize(Profile profile)
{
    if (g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    g_initialized = true;
    g_ready       = false;

    // The console is block buffered by default; a state line written just before
    // the firmware blocks or sleeps forever would otherwise never reach the host.
    setvbuf(stdout, nullptr, _IONBF, 0);

    const esp_app_desc_t* app       = esp_app_get_description();
    const std::uint32_t config_hash = fnv1a(CONFIG_M5CC_CONFIG_ID);

    if (profile == Profile::none) {
        ESP_LOGI(kTag,
                 "TEST_START test_id=%s firmware_version=%s config_id=%s config_hash=%08" PRIx32
                 " reset_reason=%s wake_cause=%s",
                 CONFIG_M5CC_TEST_ID, app->version, CONFIG_M5CC_CONFIG_ID, config_hash,
                 reset_reason_name(esp_reset_reason()), wake_cause_name(esp_sleep_get_wakeup_cause()));
    } else {
        ESP_LOGI(kTag,
                 "TEST_START test_id=%s firmware_version=%s config_id=%s config_hash=%08" PRIx32
                 " reset_reason=%s wake_cause=%s profile=%s",
                 CONFIG_M5CC_TEST_ID, app->version, CONFIG_M5CC_CONFIG_ID, config_hash,
                 reset_reason_name(esp_reset_reason()), wake_cause_name(esp_sleep_get_wakeup_cause()),
                 profile_name(profile));
    }
    console_flush();
    return ESP_OK;
}

esp_err_t mark_ready()
{
    if (!g_initialized || g_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    g_ready = true;
    ESP_LOGI(kTag, "TEST_READY");
    console_flush();
    return ESP_OK;
}

void state_entered(const char* mode, const char* extra)
{
    if (mode == nullptr) {
        return;
    }
    if (extra == nullptr || extra[0] == '\0') {
        ESP_LOGI(kTag, "STATE_ENTERED mode=%s", mode);
    } else {
        ESP_LOGI(kTag, "STATE_ENTERED mode=%s %s", mode, extra);
    }
    console_flush();
}

void report_final(FinalStatus status, const char* detail)
{
    const char* result = status == FinalStatus::pass ? "TEST_PASS" : "TEST_FAIL";
    if (detail == nullptr || detail[0] == '\0') {
        ESP_LOGI(kTag, "%s", result);
    } else {
        ESP_LOGI(kTag, "%s reason=%s", result, detail);
    }
    console_flush();
}

void hold_safe_idle()
{
    console_flush();
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

const char* reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON:
            return "power_on";
        case ESP_RST_EXT:
            return "external_pin";
        case ESP_RST_SW:
            return "software";
        case ESP_RST_PANIC:
            return "panic";
        case ESP_RST_INT_WDT:
            return "interrupt_watchdog";
        case ESP_RST_TASK_WDT:
            return "task_watchdog";
        case ESP_RST_WDT:
            return "watchdog";
        case ESP_RST_DEEPSLEEP:
            return "deep_sleep";
        case ESP_RST_BROWNOUT:
            return "brownout";
        case ESP_RST_SDIO:
            return "sdio";
        case ESP_RST_USB:
            return "usb";
        case ESP_RST_JTAG:
            return "jtag";
        case ESP_RST_EFUSE:
            return "efuse";
        case ESP_RST_PWR_GLITCH:
            return "power_glitch";
        case ESP_RST_CPU_LOCKUP:
            return "cpu_lockup";
        case ESP_RST_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* wake_cause_name(esp_sleep_wakeup_cause_t cause)
{
    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT0:
            return "ext0";
        case ESP_SLEEP_WAKEUP_EXT1:
            return "ext1";
        case ESP_SLEEP_WAKEUP_TIMER:
            return "timer";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "touchpad";
        case ESP_SLEEP_WAKEUP_ULP:
            return "ulp";
        case ESP_SLEEP_WAKEUP_GPIO:
            return "gpio";
        case ESP_SLEEP_WAKEUP_UART:
            return "uart";
        case ESP_SLEEP_WAKEUP_WIFI:
            return "wifi";
        case ESP_SLEEP_WAKEUP_COCPU:
            return "coprocessor";
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
            return "coprocessor_trap";
        case ESP_SLEEP_WAKEUP_BT:
            return "bluetooth";
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            return "undefined";
    }
}

const char* profile_name(Profile profile)
{
    switch (profile) {
        case Profile::low_power:
            return "low_power";
        case Profile::reference:
            return "reference";
        case Profile::none:
        default:
            return "none";
    }
}

Profile configured_profile()
{
#if CONFIG_M5CC_IMU_WAKE_PROFILE == 1
    return Profile::reference;
#else
    return Profile::low_power;
#endif
}

}  // namespace m5cc::test_support
