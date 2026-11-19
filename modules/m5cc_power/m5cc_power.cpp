#include "m5cc_power.hpp"

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"

namespace m5cc::power {
namespace {

constexpr char kTag[] = "m5cc_power";

Result from(m5pm1_err_t raw)
{
    return {raw == M5PM1_OK ? ESP_OK : ESP_FAIL, static_cast<int>(raw)};
}

Result from_esp(esp_err_t error)
{
    return {error, static_cast<int>(error)};
}

Result verify_failed()
{
    return {ESP_ERR_INVALID_RESPONSE, static_cast<int>(M5PM1_ERR_VERIFY_FAILED)};
}

// Charge, boost and the power LED are off in every measured state. The DCDC and
// LDO bits are left untouched: clearing DCDC_EN while running would remove the
// ESP32 supply before shutdown() is reached.
Result set_quiet_power_bits()
{
    auto* pm1                    = board::pm1();
    constexpr std::uint8_t kMask = M5PM1_PWR_CFG_CHG_EN | M5PM1_PWR_CFG_BOOST_EN | M5PM1_PWR_CFG_LED_CTRL;
    Result result                = from(pm1->setPowerConfig(kMask, 0));
    if (!result.ok()) {
        return result;
    }
    std::uint8_t config = 0;
    result              = from(pm1->getPowerConfig(&config));
    if (!result.ok()) {
        return result;
    }
    return (config & kMask) == 0 ? Result{} : verify_failed();
}

Result set_ldo_hold(bool enabled)
{
    auto* pm1     = board::pm1();
    Result result = from(pm1->ldoSetPowerHold(enabled));
    if (!result.ok()) {
        return result;
    }
    bool actual = !enabled;
    result      = from(pm1->ldoGetPowerHold(&actual));
    if (!result.ok()) {
        return result;
    }
    return actual == enabled ? Result{} : verify_failed();
}

Result configure_wake(WakeSource wake)
{
    // The keys never route through the PMIC, so there is no PMIC pin to arm.
    if (wake == WakeSource::none || wake == WakeSource::key) {
        return {};
    }
    auto* pm1      = board::pm1();
    const auto pin = wake == WakeSource::imu ? M5PM1_GPIO_NUM_4 : M5PM1_GPIO_NUM_2;
    const auto irq = wake == WakeSource::imu ? M5PM1_IRQ_GPIO4 : M5PM1_IRQ_GPIO2;

    Result result = from(pm1->irqSetGpioMask(irq, M5PM1_IRQ_MASK_ENABLE));
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->gpioSetWakeEnable(pin, false));
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->gpioSetFunc(pin, M5PM1_GPIO_FUNC_GPIO));
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->gpioSetMode(pin, M5PM1_GPIO_MODE_INPUT));
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->gpioSetPull(pin, M5PM1_GPIO_PULL_NONE));
    if (!result.ok()) {
        return result;
    }
    // Both sources produce a falling edge:
    //   RTC  - RX8130 /INT is open-drain, active low (validated by the factory
    //          firmware's 3 s shutdown-wake production test).
    //   IMU  - BMI270 INT1 is configured active low and non-latched, so asserting
    //          the interrupt pulls the line down. An idle-high level also avoids
    //          sinking the pull-up on this board's INT1 net.
    result = from(pm1->gpioSetWakeEdge(pin, M5PM1_GPIO_WAKE_FALLING));
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->gpioSetWakeEnable(pin, true));
    if (!result.ok()) {
        return result;
    }
    // The interrupt must stay unmasked: the source event is what propagates
    // through PMG1_IRQOUT to the ESP32 ext0 input in L2.
    result = from(pm1->irqSetGpioMask(irq, M5PM1_IRQ_MASK_DISABLE));
    if (!result.ok()) {
        return result;
    }

    m5pm1_pin_status_t status = {};
    result                    = from(pm1->getPinStatus(pin, &status));
    if (!result.ok()) {
        return result;
    }
    if (!status.wake_en || status.wake_edge != M5PM1_GPIO_WAKE_FALLING) {
        return verify_failed();
    }
    m5pm1_irq_mask_ctrl_t irq_mask = M5PM1_IRQ_MASK_ENABLE;
    result                         = from(pm1->irqGetGpioMask(irq, &irq_mask));
    if (!result.ok()) {
        return result;
    }
    return irq_mask == M5PM1_IRQ_MASK_DISABLE ? Result{} : verify_failed();
}

// Routes the PMIC interrupt aggregator out on GPIO1 so that the ESP32 can use it
// as ext0 wake input on GPIO14.
Result configure_irq_output()
{
    auto* pm1      = board::pm1();
    const auto pin = M5PM1_GPIO_NUM_1;
    Result result  = from(pm1->gpioSetWakeEnable(pin, false));
    if (!result.ok()) {
        return result;
    }
    // G1 is the PMIC interrupt output. The factory production firmware drives it as
    // OUTPUT + FUNC_IRQ + pull-up (app_rtc.cpp), which is the same board, so follow
    // that instead of the other reference project's INPUT setting.
    result = from(pm1->gpioSetMode(pin, M5PM1_GPIO_MODE_OUTPUT));
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->gpioSetPull(pin, M5PM1_GPIO_PULL_UP));
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->gpioSetDrive(pin, M5PM1_GPIO_DRIVE_PUSHPULL));
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->gpioSetFunc(pin, M5PM1_GPIO_FUNC_IRQ));
    if (!result.ok()) {
        return result;
    }
    m5pm1_pin_status_t status = {};
    result                    = from(pm1->getPinStatus(pin, &status));
    if (!result.ok()) {
        return result;
    }
    return status.func == M5PM1_GPIO_FUNC_IRQ ? Result{} : verify_failed();
}

Result configure_key_wake()
{
    // EXT1 wake mask bits are GPIO numbers. All three keys are RTC GPIOs on the
    // ESP32-S3, and the keys are wired straight to the chip, so the PMIC is not
    // involved in this path.
    const gpio_num_t keys[] = {board::kKey1, board::kKey2, board::kKey3};
    std::uint64_t wake_mask = 0;
    for (const gpio_num_t key : keys) {
        esp_err_t result = rtc_gpio_init(key);
        if (result != ESP_OK) {
            return from_esp(result);
        }
        result = rtc_gpio_set_direction(key, RTC_GPIO_MODE_INPUT_ONLY);
        if (result != ESP_OK) {
            return from_esp(result);
        }
        result = rtc_gpio_pulldown_dis(key);
        if (result != ESP_OK) {
            return from_esp(result);
        }
        // The board has external pull-ups; enabling the internal one as well
        // guarantees the idle-high level even if a pull-up stays unpowered.
        result = rtc_gpio_pullup_en(key);
        if (result != ESP_OK) {
            return from_esp(result);
        }
        wake_mask |= 1ULL << static_cast<std::uint32_t>(key);
    }
    const esp_err_t result = esp_sleep_enable_ext1_wakeup_io(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);
    if (result != ESP_OK) {
        return from_esp(result);
    }
    // The PMIC IRQ output is not a wake source here; isolate it so a floating
    // PMG1_IRQOUT cannot leak through GPIO14.
    rtc_gpio_isolate(board::kPm1Irq);
    return {};
}

Result configure_sleep_source(WakeSource wake)
{
    const esp_err_t disable_result = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (disable_result != ESP_OK) {
        return from_esp(disable_result);
    }
    if (wake == WakeSource::key) {
        return configure_key_wake();
    }
    if (wake != WakeSource::none) {
        esp_err_t result = rtc_gpio_init(board::kPm1Irq);
        if (result != ESP_OK) {
            return from_esp(result);
        }
        result = rtc_gpio_set_direction(board::kPm1Irq, RTC_GPIO_MODE_INPUT_ONLY);
        if (result != ESP_OK) {
            return from_esp(result);
        }
        result = rtc_gpio_pulldown_dis(board::kPm1Irq);
        if (result != ESP_OK) {
            return from_esp(result);
        }
        result = rtc_gpio_pullup_en(board::kPm1Irq);
        if (result != ESP_OK) {
            return from_esp(result);
        }
        result = esp_sleep_enable_ext0_wakeup(board::kPm1Irq, 0);
        if (result != ESP_OK) {
            return from_esp(result);
        }
    }
    // Keys carry external pull-ups; isolating them removes the leakage path.
    rtc_gpio_isolate(board::kKey1);
    rtc_gpio_isolate(board::kKey2);
    rtc_gpio_isolate(board::kKey3);
    if (wake == WakeSource::none) {
        rtc_gpio_isolate(board::kPm1Irq);
    }
    return {};
}

}  // namespace

Result enter_l0(bool keep_charge_enabled)
{
    auto* pm1 = board::pm1();
    if (pm1 == nullptr) {
        return from_esp(ESP_ERR_INVALID_STATE);
    }
    Result result = from_esp(board::apply_quiet_state());
    if (!result.ok()) {
        return result;
    }
    result = from_esp(board::pm1_safe_state());
    if (!result.ok()) {
        return result;
    }
    result = set_quiet_power_bits();
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->wdtSet(0));
    if (!result.ok()) {
        return result;
    }
    if (keep_charge_enabled) {
        // Diagnostic only: leave the charger armed so its contribution to the L0
        // current can be measured. The normal path keeps CHG_EN at zero.
        result = from(pm1->setChargeEnable(true));
        if (!result.ok()) {
            return result;
        }
        ESP_LOGW(kTag, "L0_EXPERIMENT charge_en=1");
    }
    result = set_ldo_hold(false);
    if (!result.ok()) {
        return result;
    }
    ESP_LOGD(kTag, "L0_ARMED");
    return from(pm1->shutdown());
}

Result enter_l1(WakeSource wake)
{
    auto* pm1 = board::pm1();
    if (pm1 == nullptr) {
        return from_esp(ESP_ERR_INVALID_STATE);
    }
    if (wake == WakeSource::key) {
        // The keys only reach the ESP32; from a PMIC shutdown they cannot start
        // the board again. Refuse instead of shutting down with no wake source.
        return from_esp(ESP_ERR_INVALID_ARG);
    }
    Result result = from_esp(board::apply_quiet_state());
    if (!result.ok()) {
        return result;
    }
    result = from_esp(board::pm1_safe_state());
    if (!result.ok()) {
        return result;
    }
    result = configure_wake(wake);
    if (!result.ok()) {
        return result;
    }
    result = set_quiet_power_bits();
    if (!result.ok()) {
        return result;
    }
    result = set_ldo_hold(true);
    if (!result.ok()) {
        return result;
    }
    ESP_LOGD(kTag, "L1_ARMED wake=%d", static_cast<int>(wake));
    return from(pm1->shutdown());
}

Result prepare_l2(WakeSource wake)
{
    auto* pm1 = board::pm1();
    if (pm1 == nullptr) {
        return from_esp(ESP_ERR_INVALID_STATE);
    }
    // Mask every PMIC interrupt first, then run the same rail shutdown the L2
    // standby state uses. The standby sequence is what keeps the RGB pins off the
    // PWM channels; using apply_quiet_state() here left them on PWM duty 0 and
    // cost about 470 uA in every wake-armed L2 test.
    Result result = from_esp(board::pm1_safe_state());
    if (!result.ok()) {
        return result;
    }
    result = from_esp(board::prepare_l2_quiet());
    if (!result.ok()) {
        return result;
    }
    if (wake == WakeSource::imu || wake == WakeSource::rtc) {
        // A latched wake source from the previous power-up keeps the PMIC from
        // reporting a new edge through IRQOUT, so it is cleared before arming.
        result = from(pm1->clearWakeSource(0xFF));
        if (!result.ok()) {
            return result;
        }
        result = configure_irq_output();
        if (!result.ok()) {
            return result;
        }
        result = configure_wake(wake);
        if (!result.ok()) {
            return result;
        }
        // A stale IRQ status would hold PMG1_IRQOUT low, which makes the ext0
        // low-level wake fire immediately and turns the test into a wake loop.
        result = from(pm1->irqClearGpioAll());
        if (!result.ok()) {
            return result;
        }
        result = from(pm1->irqClearSysAll());
        if (!result.ok()) {
            return result;
        }
        std::uint8_t pending = 0;
        result               = from(pm1->irqGetGpioStatus(&pending, M5PM1_CLEAN_NONE));
        if (!result.ok()) {
            return result;
        }
        if (pending != 0) {
            return verify_failed();
        }
    }
    result = configure_sleep_source(wake);
    if (!result.ok()) {
        return result;
    }
    result = set_quiet_power_bits();
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->wdtSet(0));
    if (!result.ok()) {
        return result;
    }
    return set_ldo_hold(true);
}

Result finalize_l2(bool pm1_idle_sleep)
{
    auto* ioe = board::ioe();
    auto* pm1 = board::pm1();
    if (ioe == nullptr || pm1 == nullptr) {
        return from_esp(ESP_ERR_INVALID_STATE);
    }
    if (ioe->setI2cSleepTime(1) != M5IOE1_OK) {
        return {ESP_FAIL, static_cast<int>(M5IOE1_FAIL)};
    }
    // A wake-armed L2 must keep the PMIC awake: in idle sleep it stops reporting
    // G2/G4 edges through IRQOUT, which silently turns every L2 wake test into a
    // one-way trip. The reference project passes L2Pm1IdleSleep::disabled for the
    // same reason.
    return from(pm1->setI2cSleepTime(pm1_idle_sleep ? 1 : 0));
}

void enter_deep_sleep(std::uint32_t settle_ms)
{
    if (settle_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(settle_ms));
    }
    esp_deep_sleep_start();
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

Result read_pmic_wake(std::uint8_t* wake_source, std::uint8_t* gpio_irq)
{
    auto* pm1 = board::pm1();
    if (pm1 == nullptr || wake_source == nullptr || gpio_irq == nullptr) {
        return from_esp(ESP_ERR_INVALID_ARG);
    }
    Result result = from(pm1->getWakeSource(wake_source, M5PM1_CLEAN_NONE));
    if (!result.ok()) {
        return result;
    }
    return from(pm1->irqGetGpioStatus(gpio_irq, M5PM1_CLEAN_NONE));
}

Result clear_pmic_wake()
{
    auto* pm1 = board::pm1();
    if (pm1 == nullptr) {
        return from_esp(ESP_ERR_INVALID_STATE);
    }
    Result result = from(pm1->clearWakeSource(0x7F));
    if (!result.ok()) {
        return result;
    }
    result = from(pm1->irqClearSysAll());
    if (!result.ok()) {
        return result;
    }
    return from(pm1->irqClearGpioAll());
}

}  // namespace m5cc::power
