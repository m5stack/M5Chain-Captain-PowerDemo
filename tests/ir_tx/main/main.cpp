#include <cstdint>

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5cc_rtc.hpp"
#include "m5cc_test_support.hpp"

namespace {

constexpr char kTag[]                       = "ir_tx";
constexpr TickType_t kSettleDelay           = pdMS_TO_TICKS(3000);
constexpr std::uint32_t kResolutionHz       = 1000000;  // 1 tick = 1 us
constexpr std::uint32_t kCarrierFrequencyHz = 38000;    // NEC carrier
constexpr float kCarrierDutyCycle           = 0.33F;
constexpr std::uint32_t kFramePeriodMs      = 100;  // 10 NEC frames per second
constexpr std::uint32_t kLeaderMarkUs       = 9000;
constexpr std::uint32_t kLeaderSpaceUs      = 4500;
constexpr std::uint32_t kBitMarkUs          = 560;
constexpr std::uint32_t kZeroSpaceUs        = 560;
constexpr std::uint32_t kOneSpaceUs         = 1690;
constexpr std::size_t kMaxPulses            = 80;
constexpr std::size_t kMaxSymbols           = kMaxPulses / 2;

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

// A NEC frame is built once as a mark/space list and packed into RMT symbols, so
// the measured workload is the IR carrier and not waveform construction.
struct PulseTrain {
    struct Pulse {
        bool level;
        std::uint16_t duration_us;
    };

    Pulse pulses[kMaxPulses]               = {};
    std::size_t pulse_count                = 0;
    rmt_symbol_word_t symbols[kMaxSymbols] = {};
    std::size_t symbol_count               = 0;

    void append(bool level, std::uint32_t duration_us)
    {
        if (pulse_count >= kMaxPulses) {
            return;
        }
        // RMT durations are 15-bit at 1 MHz, so anything longer is a bug.
        pulses[pulse_count].level       = level;
        pulses[pulse_count].duration_us = static_cast<std::uint16_t>(duration_us & 0x7FFFU);
        ++pulse_count;
    }

    void pack()
    {
        symbol_count = 0;
        for (std::size_t index = 0; index < pulse_count; index += 2) {
            rmt_symbol_word_t symbol = {};
            symbol.level0            = pulses[index].level ? 1U : 0U;
            symbol.duration0         = pulses[index].duration_us;
            if ((index + 1U) < pulse_count) {
                symbol.level1    = pulses[index + 1U].level ? 1U : 0U;
                symbol.duration1 = pulses[index + 1U].duration_us;
            } else {
                symbol.level1    = symbol.level0;
                symbol.duration1 = 0;
            }
            symbols[symbol_count] = symbol;
            ++symbol_count;
        }
    }
};

// One NEC frame: leader, four bytes LSB-first, then the stop mark. The inter-frame
// gap is produced by the transmit loop instead of an over-long RMT duration.
void build_nec_frame(PulseTrain* train)
{
    train->pulse_count = 0;
    train->append(true, kLeaderMarkUs);
    train->append(false, kLeaderSpaceUs);

    constexpr std::uint8_t kAddress = 0x00;
    constexpr std::uint8_t kCommand = 0x00;
    const std::uint32_t frame =
        static_cast<std::uint32_t>(kAddress) | (static_cast<std::uint32_t>(~kAddress & 0xFFU) << 8U) |
        (static_cast<std::uint32_t>(kCommand) << 16U) | (static_cast<std::uint32_t>(~kCommand & 0xFFU) << 24U);
    for (int bit = 0; bit < 32; ++bit) {
        const bool one = ((frame >> bit) & 1U) != 0U;
        train->append(true, kBitMarkUs);
        train->append(false, one ? kOneSpaceUs : kZeroSpaceUs);
    }
    train->append(true, kBitMarkUs);
    train->pack();
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

    const esp_err_t rail_result = m5cc::board::set_rail(m5cc::board::Rail::ir, true);
    if (rail_result != ESP_OK) {
        fail_board("ir_rail", rail_result);
    }

    rmt_tx_channel_config_t channel_config = {};
    channel_config.gpio_num                = m5cc::board::kIrTx;
    channel_config.clk_src                 = RMT_CLK_SRC_DEFAULT;
    channel_config.resolution_hz           = kResolutionHz;
    channel_config.mem_block_symbols       = 64;
    channel_config.trans_queue_depth       = 4;
    rmt_channel_handle_t channel           = nullptr;
    esp_err_t result                       = rmt_new_tx_channel(&channel_config, &channel);
    if (result != ESP_OK) {
        fail_board("rmt_channel", result);
    }

    rmt_carrier_config_t carrier_config = {};
    carrier_config.frequency_hz         = kCarrierFrequencyHz;
    carrier_config.duty_cycle           = kCarrierDutyCycle;
    result                              = rmt_apply_carrier(channel, &carrier_config);
    if (result != ESP_OK) {
        fail_board("rmt_carrier", result);
    }

    rmt_copy_encoder_config_t encoder_config = {};
    rmt_encoder_handle_t encoder             = nullptr;
    result                                   = rmt_new_copy_encoder(&encoder_config, &encoder);
    if (result != ESP_OK) {
        fail_board("rmt_encoder", result);
    }

    result = rmt_enable(channel);
    if (result != ESP_OK) {
        fail_board("rmt_enable", result);
    }

    static PulseTrain train;
    build_nec_frame(&train);

    ESP_LOGI(kTag, "BOARD_OK PM1=1 IOE=1 RTC=1 IR=nec carrier_hz=%u duty=0.33 frames_per_s=%u symbols=%u",
             static_cast<unsigned>(kCarrierFrequencyHz), static_cast<unsigned>(1000U / kFramePeriodMs),
             static_cast<unsigned>(train.symbol_count));

    if (m5cc::test_support::mark_ready() != ESP_OK) {
        fail_state("test_ready", ESP_ERR_INVALID_STATE);
    }
    vTaskDelay(kSettleDelay);

    m5cc::test_support::state_entered("ir_tx", "nec_38khz_10hz display=0");

    for (;;) {
        rmt_transmit_config_t transmit_config = {};
        transmit_config.loop_count            = 0;
        const esp_err_t transmit_result       = rmt_transmit(
            channel, encoder, train.symbols, train.symbol_count * sizeof(rmt_symbol_word_t), &transmit_config);
        if (transmit_result != ESP_OK) {
            ESP_LOGW(kTag, "IR_TX skip error=%s", esp_err_to_name(transmit_result));
        }
        rmt_tx_wait_all_done(channel, 1000);
        vTaskDelay(pdMS_TO_TICKS(kFramePeriodMs));
    }
}
