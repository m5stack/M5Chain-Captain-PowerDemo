#include "m5cc_board.hpp"

#include <cstdint>

#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "m5stack_chain_captain.h"

namespace m5cc::board {
namespace {

constexpr char kTag[] = "m5cc_board";

M5PM1* g_pm1                  = nullptr;
M5IOE1* g_ioe                 = nullptr;
i2c_master_bus_handle_t g_bus = nullptr;
bool g_initialized            = false;

esp_err_t map_ioe(m5ioe1_err_t error)
{
    return error == M5IOE1_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t map_pm1(m5pm1_err_t error)
{
    return error == M5PM1_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t drive_pin_low(gpio_num_t pin)
{
    gpio_config_t config = {};
    config.pin_bit_mask  = 1ULL << static_cast<std::uint32_t>(pin);
    config.mode          = GPIO_MODE_OUTPUT;
    config.pull_up_en    = GPIO_PULLUP_DISABLE;
    config.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    config.intr_type     = GPIO_INTR_DISABLE;
    const esp_err_t err  = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    return gpio_set_level(pin, 0);
}

esp_err_t set_pm1_port_enable(m5pm1_gpio_num_t pin, bool enabled)
{
    esp_err_t err = map_pm1(g_pm1->gpioSetDrive(pin, M5PM1_GPIO_DRIVE_PUSHPULL));
    if (err != ESP_OK) {
        return err;
    }
    err = map_pm1(g_pm1->gpioSetMode(pin, M5PM1_GPIO_MODE_OUTPUT));
    if (err != ESP_OK) {
        return err;
    }
    err = map_pm1(g_pm1->gpioSetOutput(pin, enabled ? 1 : 0));
    if (err != ESP_OK) {
        return err;
    }
    m5pm1_pin_status_t status = {};
    err                       = map_pm1(g_pm1->getPinStatus(pin, &status));
    if (err != ESP_OK) {
        return err;
    }
    if (status.mode != M5PM1_GPIO_MODE_OUTPUT || status.output != (enabled ? 1 : 0)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

}  // namespace

esp_err_t initialize()
{
    if (g_initialized) {
        return ESP_OK;
    }
    const esp_err_t bsp_result = bsp_init();
    if (bsp_result != ESP_OK) {
        ESP_LOGE(kTag, "BOARD_FAIL stage=bsp_init error=%s", esp_err_to_name(bsp_result));
        return bsp_result;
    }
    g_pm1 = bsp_pmic_get_handle();
    g_ioe = bsp_ioe_get_handle();
    g_bus = bsp_i2c_get_handle(BSP_SYS_I2C_NUM);
    if (g_pm1 == nullptr || g_ioe == nullptr || g_bus == nullptr) {
        ESP_LOGE(kTag, "BOARD_FAIL stage=bsp_handles pm1=%d ioe=%d", g_pm1 != nullptr, g_ioe != nullptr);
        g_pm1 = nullptr;
        g_ioe = nullptr;
        g_bus = nullptr;
        return ESP_ERR_INVALID_STATE;
    }
    g_initialized = true;

    const esp_err_t pwm_result = map_ioe(g_ioe->setPwmFrequency(kSharedPwmFrequencyHz));
    if (pwm_result != ESP_OK) {
        ESP_LOGE(kTag, "BOARD_FAIL stage=pwm_frequency error=%s", esp_err_to_name(pwm_result));
        return pwm_result;
    }
    const esp_err_t quiet_result = apply_quiet_state();
    if (quiet_result != ESP_OK) {
        ESP_LOGE(kTag, "BOARD_FAIL stage=quiet_state error=%s", esp_err_to_name(quiet_result));
        return quiet_result;
    }
    ESP_LOGD(kTag, "BOARD_QUIET pm1=1 ioe=1 ioe_irq=disabled chg=0 boost=0 led=0 pa=0");
    return ESP_OK;
}

esp_err_t initialize_for_l2()
{
    if (g_initialized) {
        return ESP_OK;
    }
    const esp_err_t bsp_result = bsp_init();
    if (bsp_result != ESP_OK) {
        return bsp_result;
    }
    g_pm1 = bsp_pmic_get_handle();
    g_ioe = bsp_ioe_get_handle();
    g_bus = bsp_i2c_get_handle(BSP_SYS_I2C_NUM);
    if (g_pm1 == nullptr || g_ioe == nullptr || g_bus == nullptr) {
        g_pm1 = nullptr;
        g_ioe = nullptr;
        g_bus = nullptr;
        return ESP_ERR_INVALID_STATE;
    }
    g_initialized = true;
    return ESP_OK;
}

esp_err_t prepare_l2_quiet()
{
    auto* pm1 = g_pm1;
    auto* ioe = g_ioe;
    if (pm1 == nullptr || ioe == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    // Power LED and charger off.
    pm1->setLedEnLevel(false);
    pm1->setChargeEnable(false);

    // Charge indicator driven high, boost switch off.
    ioe->pinMode(M5IOE1_PIN_3, OUTPUT);
    ioe->setDriveMode(M5IOE1_PIN_3, M5IOE1_DRIVE_PUSHPULL);
    ioe->digitalWrite(M5IOE1_PIN_3, HIGH);
    ioe->pinMode(M5IOE1_PIN_4, OUTPUT);
    ioe->digitalWrite(M5IOE1_PIN_4, LOW);

    // PWM stopped entirely, then the LCD/audio/IR rails switched off.
    ioe->setPwmFrequency(0);
    ioe->setPwmDuty(M5IOE1_PWM_CH3, 0, false, false);
    ioe->pinMode(M5IOE1_PIN_11, OUTPUT);
    ioe->setDriveMode(M5IOE1_PIN_11, M5IOE1_DRIVE_PUSHPULL);
    ioe->digitalWrite(M5IOE1_PIN_11, LOW);
    ioe->pinMode(M5IOE1_PIN_1, OUTPUT);
    ioe->setDriveMode(M5IOE1_PIN_1, M5IOE1_DRIVE_PUSHPULL);
    ioe->digitalWrite(M5IOE1_PIN_1, LOW);
    const std::uint8_t feature_pins[] = {M5IOE1_PIN_12, M5IOE1_PIN_5, M5IOE1_PIN_2};
    for (const std::uint8_t pin : feature_pins) {
        ioe->pinMode(pin, OUTPUT);
        ioe->setDriveMode(pin, M5IOE1_DRIVE_PUSHPULL);
        ioe->digitalWrite(pin, LOW);
    }

    // The RGB pins are held at 0 V as plain outputs. Leaving them on a PWM channel
    // at duty 0 still conducts, and leaving them floating lets the illuminated LED
    // leak; both cost 150 uA or more. A driven low level is what keeps L2 at about
    // 94 uA regardless of ambient light. Every L2 state (standby or wake-armed)
    // must go through this sequence.
    const std::uint8_t rgb_pins[] = {kIoeRgbRed, kIoeRgbGreen, kIoeRgbBlue};
    for (const std::uint8_t pin : rgb_pins) {
        ioe->pinMode(pin, OUTPUT);
        ioe->setDriveMode(pin, M5IOE1_DRIVE_PUSHPULL);
        ioe->digitalWrite(pin, LOW);
    }

    // 5 V output low, boost off, keep the IMU rail powered.
    pm1->gpioSetDrive(M5PM1_GPIO_NUM_3, M5PM1_GPIO_DRIVE_PUSHPULL);
    pm1->gpioSetMode(M5PM1_GPIO_NUM_3, M5PM1_GPIO_MODE_OUTPUT);
    pm1->gpioSetOutput(M5PM1_GPIO_NUM_3, false);
    pm1->setBoostEnable(false);
    pm1->ldoSetPowerHold(true);
    return ESP_OK;
}

void enter_l2_standby()
{
    auto* pm1 = g_pm1;
    auto* ioe = g_ioe;
    if (pm1 == nullptr || ioe == nullptr) {
        for (;;) {
            vTaskDelay(portMAX_DELAY);
        }
    }

    prepare_l2_quiet();

    // Expander idle sleep, then a settling second with no bus traffic.
    ioe->setI2cSleepTime(5);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // PMIC idle sleep, IRQ output parked low, then one more silent second.
    pm1->setI2cSleepTime(5);
    pm1->gpioSetDrive(M5PM1_GPIO_NUM_1, M5PM1_GPIO_DRIVE_PUSHPULL);
    pm1->gpioSetMode(M5PM1_GPIO_NUM_1, M5PM1_GPIO_MODE_OUTPUT);
    pm1->gpioSetOutput(M5PM1_GPIO_NUM_1, false);
    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_deep_sleep_start();
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

bool initialized()
{
    return g_initialized;
}

M5PM1* pm1()
{
    return g_pm1;
}

M5IOE1* ioe()
{
    return g_ioe;
}

i2c_master_bus_handle_t i2c_bus()
{
    return g_bus;
}

esp_err_t disable_ioe_interrupts()
{
    if (!g_initialized || g_ioe == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return map_ioe(g_ioe->setInterruptMode(M5IOE1_INT_MODE_DISABLED));
}

esp_err_t pm1_safe_state()
{
    if (!g_initialized || g_pm1 == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = map_pm1(g_pm1->irqSetGpioMaskAll(M5PM1_IRQ_MASK_ENABLE));
    if (err != ESP_OK) {
        return err;
    }
    err = map_pm1(g_pm1->irqSetSysMaskAll(M5PM1_IRQ_MASK_ENABLE));
    if (err != ESP_OK) {
        return err;
    }
    err = map_pm1(g_pm1->irqSetBtnMaskAll(M5PM1_IRQ_MASK_ENABLE));
    if (err != ESP_OK) {
        return err;
    }

    for (std::uint8_t index = 0; index < M5PM1_MAX_GPIO_PINS; ++index) {
        const auto pin = static_cast<m5pm1_gpio_num_t>(index);
        err            = map_pm1(g_pm1->gpioSetWakeEnable(pin, false));
        if (err != ESP_OK) {
            return err;
        }
        err = map_pm1(g_pm1->gpioSetFunc(pin, M5PM1_GPIO_FUNC_GPIO));
        if (err != ESP_OK) {
            return err;
        }
        err = map_pm1(g_pm1->gpioSetMode(pin, M5PM1_GPIO_MODE_INPUT));
        if (err != ESP_OK) {
            return err;
        }
        err = map_pm1(g_pm1->gpioSetPull(pin, M5PM1_GPIO_PULL_NONE));
        if (err != ESP_OK) {
            return err;
        }
        err = map_pm1(g_pm1->gpioSetDrive(pin, M5PM1_GPIO_DRIVE_OPENDRAIN));
        if (err != ESP_OK) {
            return err;
        }
    }

    // The 5 V port enables must be driven low, not left floating.
    err = set_pm1_port_enable(M5PM1_GPIO_NUM_0, false);
    if (err != ESP_OK) {
        return err;
    }
    err = set_pm1_port_enable(M5PM1_GPIO_NUM_3, false);
    if (err != ESP_OK) {
        return err;
    }
    // Compares the cached pin configuration against the PMIC registers.
    return map_pm1(g_pm1->verifyPinConfig(false));
}

esp_err_t apply_quiet_state()
{
    if (!g_initialized || g_pm1 == nullptr || g_ioe == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = disable_ioe_interrupts();
    if (err != ESP_OK) {
        return err;
    }

    // Connect the loads after their rails are gone, never before.
    const std::uint8_t load_pins[] = {kIoeTftRail, kIoeCodecRail, kIoeIrRail, kIoeBoostRail, kIoeHeader5v};
    for (const std::uint8_t pin : load_pins) {
        err = ioe_output(pin, false);
        if (err != ESP_OK) {
            return err;
        }
    }
    err = ioe_output(kIoeTftReset, false);
    if (err != ESP_OK) {
        return err;
    }

    err = ioe_pwm(kBacklightPwmChannel, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = ioe_output(kIoeBacklight, false);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_rgb_outputs();
    if (err != ESP_OK) {
        return err;
    }

    // Charge status is an input; SWC/SWD and the unused pin stay high impedance.
    err = ioe_input(kIoeChargeState, true);
    if (err != ESP_OK) {
        return err;
    }
    const std::uint8_t idle_pins[] = {kIoeUnused, kIoeSwc, kIoeSwd};
    for (const std::uint8_t pin : idle_pins) {
        err = ioe_input(pin, false);
        if (err != ESP_OK) {
            return err;
        }
    }

    err = pm1_safe_state();
    if (err != ESP_OK) {
        return err;
    }
    err = map_pm1(g_pm1->setChargeEnable(false));
    if (err != ESP_OK) {
        return err;
    }
    err = map_pm1(g_pm1->setBoostEnable(false));
    if (err != ESP_OK) {
        return err;
    }
    err = map_pm1(g_pm1->setLedEnLevel(false));
    if (err != ESP_OK) {
        return err;
    }

    std::uint8_t power_config         = 0;
    constexpr std::uint8_t kQuietMask = M5PM1_PWR_CFG_CHG_EN | M5PM1_PWR_CFG_BOOST_EN | M5PM1_PWR_CFG_LED_CTRL;
    err                               = map_pm1(g_pm1->getPowerConfig(&power_config));
    if (err != ESP_OK) {
        return err;
    }
    if ((power_config & kQuietMask) != 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    err = map_pm1(g_pm1->wdtSet(0));
    if (err != ESP_OK) {
        return err;
    }
    err = map_pm1(g_pm1->setI2cSleepTime(0));
    if (err != ESP_OK) {
        return err;
    }
    err = configure_pa_off();
    if (err != ESP_OK) {
        return err;
    }
    return configure_ir_tx_off();
}

esp_err_t ioe_output(std::uint8_t pin, bool level)
{
    if (!g_initialized || g_ioe == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    m5ioe1_err_t raw = M5IOE1_OK;
    g_ioe->pinModeWithRes(pin, OUTPUT, &raw);
    if (raw != M5IOE1_OK) {
        return ESP_FAIL;
    }
    const esp_err_t drive_result = map_ioe(g_ioe->setDriveMode(pin, M5IOE1_DRIVE_PUSHPULL));
    if (drive_result != ESP_OK) {
        return drive_result;
    }
    g_ioe->digitalWriteWithRes(pin, level ? 1 : 0, &raw);
    if (raw != M5IOE1_OK) {
        return ESP_FAIL;
    }
    const int readback = g_ioe->digitalReadWithRes(pin, &raw);
    if (raw != M5IOE1_OK) {
        return ESP_FAIL;
    }
    return readback == (level ? 1 : 0) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t ioe_input(std::uint8_t pin, bool pull_up)
{
    if (!g_initialized || g_ioe == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    m5ioe1_err_t raw = M5IOE1_OK;
    g_ioe->pinModeWithRes(pin, pull_up ? INPUT_PULLUP : INPUT, &raw);
    return raw == M5IOE1_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t ioe_pwm(std::uint8_t channel, std::uint8_t duty)
{
    if (!g_initialized || g_ioe == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return map_ioe(g_ioe->setPwmDuty(channel, duty));
}

esp_err_t set_rail(Rail rail, bool enabled)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    switch (rail) {
        case Rail::tft_logic:
            return ioe_output(kIoeTftRail, enabled);
        case Rail::codec:
            return ioe_output(kIoeCodecRail, enabled);
        case Rail::ir:
            return ioe_output(kIoeIrRail, enabled);
        case Rail::header_5v:
            return ioe_output(kIoeHeader5v, enabled);
        case Rail::boost_5v: {
            const esp_err_t err = ioe_output(kIoeBoostRail, enabled);
            if (err != ESP_OK) {
                return err;
            }
            return map_pm1(g_pm1->setBoostEnable(enabled));
        }
        case Rail::grove_5v:
            return set_pm1_port_enable(M5PM1_GPIO_NUM_0, enabled);
        case Rail::chain_5v:
            return set_pm1_port_enable(M5PM1_GPIO_NUM_3, enabled);
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t set_backlight(std::uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    if (percent == 0) {
        const esp_err_t err = ioe_pwm(kBacklightPwmChannel, 0);
        if (err != ESP_OK) {
            return err;
        }
        return ioe_output(kIoeBacklight, false);
    }
    const esp_err_t err = ioe_output(kIoeBacklight, true);
    if (err != ESP_OK) {
        return err;
    }
    return ioe_pwm(kBacklightPwmChannel, percent);
}

esp_err_t configure_rgb_outputs()
{
    // Same sequence the factory firmware uses: push-pull output, driven high,
    // then the PWM duty decides the brightness.
    const std::uint8_t rgb_pins[] = {kIoeRgbRed, kIoeRgbGreen, kIoeRgbBlue};
    for (const std::uint8_t pin : rgb_pins) {
        const esp_err_t err = ioe_output(pin, true);
        if (err != ESP_OK) {
            return err;
        }
    }
    return set_rgb(0, 0, 0);
}

esp_err_t set_rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
{
    esp_err_t err = ioe_pwm(kRgbRedPwmChannel, red);
    if (err != ESP_OK) {
        return err;
    }
    err = ioe_pwm(kRgbGreenPwmChannel, green);
    if (err != ESP_OK) {
        return err;
    }
    return ioe_pwm(kRgbBluePwmChannel, blue);
}

esp_err_t configure_pa_off()
{
    return drive_pin_low(kPaEnable);
}

esp_err_t configure_ir_tx_off()
{
    return drive_pin_low(kIrTx);
}

}  // namespace m5cc::board
