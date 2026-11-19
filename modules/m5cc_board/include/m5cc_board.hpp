#pragma once

#include <cstdint>

#include "M5IOE1.h"
#include "M5PM1.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

namespace m5cc::board {

// M5IOE1 pin indices: M5IOE1_PIN_n is the pin index n-1.
inline constexpr std::uint8_t kIoeTftReset    = M5IOE1_PIN_1;
inline constexpr std::uint8_t kIoeIrRail      = M5IOE1_PIN_2;
inline constexpr std::uint8_t kIoeChargeState = M5IOE1_PIN_3;
inline constexpr std::uint8_t kIoeBoostRail   = M5IOE1_PIN_4;
inline constexpr std::uint8_t kIoeCodecRail   = M5IOE1_PIN_5;
inline constexpr std::uint8_t kIoeHeader5v    = M5IOE1_PIN_6;
inline constexpr std::uint8_t kIoeUnused      = M5IOE1_PIN_7;
inline constexpr std::uint8_t kIoeRgbGreen    = M5IOE1_PIN_8;
inline constexpr std::uint8_t kIoeRgbBlue     = M5IOE1_PIN_9;
inline constexpr std::uint8_t kIoeRgbRed      = M5IOE1_PIN_10;
inline constexpr std::uint8_t kIoeBacklight   = M5IOE1_PIN_11;
inline constexpr std::uint8_t kIoeTftRail     = M5IOE1_PIN_12;
inline constexpr std::uint8_t kIoeSwc         = M5IOE1_PIN_13;
inline constexpr std::uint8_t kIoeSwd         = M5IOE1_PIN_14;

// ESP32-S3 pins that must never float high or drive into an unpowered rail.
inline constexpr gpio_num_t kPaEnable = GPIO_NUM_21;
inline constexpr gpio_num_t kIrTx     = GPIO_NUM_48;
inline constexpr gpio_num_t kPm1Irq   = GPIO_NUM_14;
inline constexpr gpio_num_t kKey1     = GPIO_NUM_1;
inline constexpr gpio_num_t kKey2     = GPIO_NUM_4;
inline constexpr gpio_num_t kKey3     = GPIO_NUM_5;

inline constexpr std::uint8_t kBacklightPwmChannel = M5IOE1_PWM_CH3;
inline constexpr std::uint8_t kRgbGreenPwmChannel  = M5IOE1_PWM_CH2;
inline constexpr std::uint8_t kRgbBluePwmChannel   = M5IOE1_PWM_CH1;
inline constexpr std::uint8_t kRgbRedPwmChannel    = M5IOE1_PWM_CH4;
// M5IOE1 shares one PWM frequency between the backlight and the RGB channels.
inline constexpr std::uint16_t kSharedPwmFrequencyHz = 20000;

enum class Rail : std::uint8_t {
    tft_logic,
    codec,
    ir,
    boost_5v,
    header_5v,
    grove_5v,
    chain_5v,
};

// Runs the board support package and applies the measurement quiet state.
esp_err_t initialize();
// Bring-up for the L2 standby path: bsp_init only, so the L2 sequence itself
// owns every rail and service change.
esp_err_t initialize_for_l2();
// Shuts every rail and service down for L2 (LED, charger, boost, backlight, LCD,
// audio, IR, and the RGB pins held low) without sleeping. Shared by the L2 standby
// state and every L2 wake-armed state.
esp_err_t prepare_l2_quiet();
// Quiet state above plus the idle-sleep timers, then deep sleep. Never returns.
[[noreturn]] void enter_l2_standby();
bool initialized();

M5PM1* pm1();
M5IOE1* ioe();
i2c_master_bus_handle_t i2c_bus();

// Stops the IO expander polling task; the factory BSP starts it in POLLING mode.
esp_err_t disable_ioe_interrupts();

// Drives every rail, port enable and indicator to the off state defined in the
// plan (section 3.8 / 3.9). Idempotent.
esp_err_t apply_quiet_state();

// PMIC-only part of the quiet state: IRQs masked, every PMIC GPIO a benign
// input, then the Grove / Chain port enables driven low.
esp_err_t pm1_safe_state();

esp_err_t ioe_output(std::uint8_t pin, bool level);
esp_err_t ioe_input(std::uint8_t pin, bool pull_up);
esp_err_t ioe_pwm(std::uint8_t channel, std::uint8_t duty);

esp_err_t set_rail(Rail rail, bool enabled);
esp_err_t set_backlight(std::uint8_t percent);
// The RGB pins must be outputs before any PWM duty has an effect.
esp_err_t configure_rgb_outputs();
esp_err_t set_rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue);

// PA (AW8737A) and IR transmitter drivers off, as required by section 3.9.
esp_err_t configure_pa_off();
esp_err_t configure_ir_tx_off();

}  // namespace m5cc::board
