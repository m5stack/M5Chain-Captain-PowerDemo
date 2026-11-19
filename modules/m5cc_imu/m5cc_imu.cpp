#include "m5cc_imu.hpp"

#include <cstdint>

#include "accel_gyro_bmi270.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "bmi2_defs.h"
#include "bmi270.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"

namespace m5cc::imu {
namespace {

constexpr char kTag[]                   = "m5cc_imu";
constexpr std::uint8_t kChipId          = 0x24;
constexpr std::uint8_t kSensorPowerMask = BMI2_AUX_EN_MASK | BMI2_GYR_EN_MASK | BMI2_ACC_EN_MASK | BMI2_TEMP_EN_MASK;

// Values validated by the C160 factory firmware (see components/sensor_bmi270).
constexpr std::uint8_t kAnyMotionDuration  = 0x08;  // 8 x 20 ms = 160 ms
constexpr std::uint8_t kAnyMotionThreshold = 0x68;  // 50 mg

// Accelerometer ODR for the low-power wake profile. any-motion samples at 50 Hz,
// so 50 Hz is the default; the lower rates exist to measure how far the current
// drops when wake-up responsiveness is traded away. The any-motion duration is
// deliberately not rescaled, so the effective detection time grows as the ODR drops.
#if CONFIG_M5CC_IMU_ODR_0_78HZ
constexpr std::uint8_t kAccelOdr = BMI2_ACC_ODR_0_78HZ;
constexpr char kAccelOdrLabel[]  = "0.78 Hz";
#elif CONFIG_M5CC_IMU_ODR_1_5HZ
constexpr std::uint8_t kAccelOdr = BMI2_ACC_ODR_1_56HZ;
constexpr char kAccelOdrLabel[]  = "1.5 Hz";
#elif CONFIG_M5CC_IMU_ODR_3_1HZ
constexpr std::uint8_t kAccelOdr = BMI2_ACC_ODR_3_12HZ;
constexpr char kAccelOdrLabel[]  = "3.1 Hz";
#elif CONFIG_M5CC_IMU_ODR_6_25HZ
constexpr std::uint8_t kAccelOdr = BMI2_ACC_ODR_6_25HZ;
constexpr char kAccelOdrLabel[]  = "6.25 Hz";
#elif CONFIG_M5CC_IMU_ODR_12_5HZ
constexpr std::uint8_t kAccelOdr = BMI2_ACC_ODR_12_5HZ;
constexpr char kAccelOdrLabel[]  = "12.5 Hz";
#elif CONFIG_M5CC_IMU_ODR_25HZ
constexpr std::uint8_t kAccelOdr = BMI2_ACC_ODR_25HZ;
constexpr char kAccelOdrLabel[]  = "25 Hz";
#else
constexpr std::uint8_t kAccelOdr = BMI2_ACC_ODR_50HZ;
constexpr char kAccelOdrLabel[]  = "50 Hz";
#endif

bool g_initialized               = false;
bool g_any_motion                = false;
TaskHandle_t g_sample_task       = nullptr;
TickType_t g_sample_period_ticks = 1;
std::uint32_t g_sample_count     = 0;

Result from_bmi(int8_t code)
{
    return {code == BMI2_OK ? ESP_OK : ESP_FAIL, static_cast<int>(code)};
}

struct bmi2_dev* device()
{
    return accel_gyro_bmi270_get_dev();
}

Result configure_sensor_load(struct bmi2_dev* dev, WakeProfile profile)
{
    struct bmi2_sens_config config[2] = {};
    config[0].type                    = BMI2_ACCEL;
    const std::uint8_t count          = profile == WakeProfile::reference ? 2 : 1;
    if (count == 2) {
        config[1].type = BMI2_GYRO;
    }

    Result result = from_bmi(bmi270_get_sensor_config(config, count, dev));
    if (!result.ok()) {
        return result;
    }

    if (profile == WakeProfile::reference) {
        config[0].cfg.acc.odr         = BMI2_ACC_ODR_100HZ;
        config[0].cfg.acc.range       = BMI2_ACC_RANGE_2G;
        config[0].cfg.acc.bwp         = BMI2_ACC_OSR2_AVG2;
        config[0].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
        config[1].cfg.gyr.odr         = BMI2_GYR_ODR_100HZ;
        config[1].cfg.gyr.range       = BMI2_GYR_RANGE_2000;
        config[1].cfg.gyr.bwp         = BMI2_GYR_OSR2_MODE;
        config[1].cfg.gyr.noise_perf  = BMI2_PERF_OPT_MODE;
        config[1].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;
    } else {
        config[0].cfg.acc.odr   = kAccelOdr;
        config[0].cfg.acc.range = BMI2_ACC_RANGE_2G;
        // OSR4/AVG1 is the lowest-current accelerometer filter.
        config[0].cfg.acc.bwp         = BMI2_ACC_OSR4_AVG1;
        config[0].cfg.acc.filter_perf = BMI2_POWER_OPT_MODE;
    }
    return from_bmi(bmi270_set_sensor_config(config, count, dev));
}

void sampling_task(void*)
{
    struct bmi2_sens_data data = {};
    for (;;) {
        struct bmi2_dev* dev = device();
        if (dev != nullptr && bmi2_get_sensor_data(&data, dev) == BMI2_OK) {
            ++g_sample_count;
        }
        vTaskDelay(g_sample_period_ticks);
    }
}

}  // namespace

Result begin()
{
    if (g_initialized) {
        return {};
    }
    const esp_err_t init_result = accel_gyro_bmi270_init(board::i2c_bus());
    if (init_result != ESP_OK) {
        return {init_result, static_cast<int>(init_result)};
    }
    struct bmi2_dev* dev = device();
    if (dev == nullptr) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    if (dev->chip_id != kChipId) {
        return {ESP_ERR_NOT_FOUND, static_cast<int>(dev->chip_id)};
    }
    g_initialized = true;
    return {};
}

bool initialized()
{
    return g_initialized;
}

Result suspend()
{
    if (!g_initialized) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    struct bmi2_dev* dev = device();
    if (dev == nullptr) {
        return {ESP_ERR_INVALID_STATE, 0};
    }

    const std::uint8_t sensors[] = {BMI2_ACCEL, BMI2_GYRO, BMI2_AUX};
    Result result                = from_bmi(bmi270_sensor_disable(sensors, 3, dev));
    if (!result.ok()) {
        return result;
    }
    result = from_bmi(bmi2_set_adv_power_save(BMI2_ENABLE, dev));
    if (!result.ok()) {
        return result;
    }
    // The FIFO self-wake-up block is unused here and keeps extra logic clocked,
    // so PWR_CONF is left at 0x01 (advanced power save on, self-wake off).
    result = from_bmi(bmi2_set_fifo_self_wake_up(BMI2_DISABLE, dev));
    if (!result.ok()) {
        return result;
    }
    g_any_motion = false;

    std::uint8_t power_ctrl = 0;
    result                  = from_bmi(bmi2_get_regs(BMI2_PWR_CTRL_ADDR, &power_ctrl, 1, dev));
    if (!result.ok()) {
        return result;
    }
    if ((power_ctrl & kSensorPowerMask) != 0) {
        return {ESP_ERR_INVALID_RESPONSE, static_cast<int>(power_ctrl)};
    }

    return {};
}

Result configure_any_motion(WakeProfile profile)
{
    if (!g_initialized) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    struct bmi2_dev* dev = device();
    if (dev == nullptr) {
        return {ESP_ERR_INVALID_STATE, 0};
    }

    // The advanced power-save mode must be off before the feature engine is loaded.
    Result result = from_bmi(bmi2_set_adv_power_save(BMI2_DISABLE, dev));
    if (!result.ok()) {
        return result;
    }
    result = configure_sensor_load(dev, profile);
    if (!result.ok()) {
        return result;
    }

    const std::uint8_t sensors_low_power[] = {BMI2_ACCEL, BMI2_ANY_MOTION};
    const std::uint8_t sensors_reference[] = {BMI2_ACCEL, BMI2_GYRO, BMI2_ANY_MOTION};
    result = from_bmi(profile == WakeProfile::reference ? bmi270_sensor_enable(sensors_reference, 3, dev)
                                                        : bmi270_sensor_enable(sensors_low_power, 2, dev));
    if (!result.ok()) {
        return result;
    }

    struct bmi2_sens_config feature = {};
    feature.type                    = BMI2_ANY_MOTION;
    result                          = from_bmi(bmi270_get_sensor_config(&feature, 1, dev));
    if (!result.ok()) {
        return result;
    }
    feature.cfg.any_motion.duration  = kAnyMotionDuration;
    feature.cfg.any_motion.threshold = kAnyMotionThreshold;
    result                           = from_bmi(bmi270_set_sensor_config(&feature, 1, dev));
    if (!result.ok()) {
        return result;
    }

    // The feature configuration can only be written with advanced power save off.
    // Once the feature and the sensors are in place, turn it back on: it is the
    // mechanism that brings the standing accelerometer current down from the
    // 150 uA range to a few tens of microamps at 50 Hz.
    result = from_bmi(bmi2_set_adv_power_save(BMI2_ENABLE, dev));
    if (!result.ok()) {
        return result;
    }
    // The FIFO self-wake-up block is unused here and keeps extra logic clocked,
    // so PWR_CONF is left at 0x01 (advanced power save on, self-wake off).
    result = from_bmi(bmi2_set_fifo_self_wake_up(BMI2_DISABLE, dev));
    if (!result.ok()) {
        return result;
    }

    // Report the registers that decide the standing current: ACC_CONF carries the
    // accelerometer filter mode and ODR, PWR_CTRL the enabled sensors, PWR_CONF the
    // advanced power-save bit.
    std::uint8_t acc_conf   = 0;
    std::uint8_t power_ctrl = 0;
    std::uint8_t power_conf = 0;
    if (bmi2_get_regs(BMI2_ACC_CONF_ADDR, &acc_conf, 1, dev) == BMI2_OK &&
        bmi2_get_regs(BMI2_PWR_CTRL_ADDR, &power_ctrl, 1, dev) == BMI2_OK &&
        bmi2_get_regs(BMI2_PWR_CONF_ADDR, &power_conf, 1, dev) == BMI2_OK) {
        ESP_LOGI(kTag,
                 "IMU_ARMED acc_conf=0x%02X filter_perf=%u bwp=%u odr=%u pwr_ctrl=0x%02X pwr_conf=0x%02X "
                 "adv_pow_save=%u",
                 acc_conf, static_cast<unsigned>(acc_conf >> 7), static_cast<unsigned>((acc_conf >> 4) & 0x07U),
                 static_cast<unsigned>(acc_conf & 0x0FU), power_ctrl, power_conf,
                 static_cast<unsigned>(power_conf & 0x01U));
    }

    // The FIFO streams and the interrupt pin configuration also draw current. Log
    // them, then make sure no FIFO stream is left enabled for this wake-only state.
    std::uint8_t fifo_downs = 0;
    std::uint8_t fifo_cfg0  = 0;
    std::uint8_t fifo_cfg1  = 0;
    std::uint8_t int1_io    = 0;
    bmi2_get_regs(BMI2_FIFO_DOWNS_ADDR, &fifo_downs, 1, dev);
    bmi2_get_regs(BMI2_FIFO_CONFIG_0_ADDR, &fifo_cfg0, 1, dev);
    bmi2_get_regs(BMI2_FIFO_CONFIG_1_ADDR, &fifo_cfg1, 1, dev);
    bmi2_get_regs(BMI2_INT1_IO_CTRL_ADDR, &int1_io, 1, dev);
    ESP_LOGI(kTag, "IMU_FIFO before downs=0x%02X cfg0=0x%02X cfg1=0x%02X int1_io=0x%02X", fifo_downs, fifo_cfg0,
             fifo_cfg1, int1_io);
    if (bmi2_set_fifo_config(BMI2_FIFO_ALL_EN, BMI2_DISABLE, dev) == BMI2_OK) {
        bmi2_get_regs(BMI2_FIFO_CONFIG_1_ADDR, &fifo_cfg1, 1, dev);
        ESP_LOGI(kTag, "IMU_FIFO after cfg1=0x%02X", fifo_cfg1);
    } else {
        ESP_LOGW(kTag, "IMU_FIFO disable_failed");
    }

    // The auxiliary I2C master (ASDx/ASCx) points at an unpopulated magnetometer.
    // Its enable bit and the ASDA pull-up selection are the two settings that can
    // leak current on those floating nets, so report both registers verbatim.
    std::uint8_t aux_if_conf = 0;
    std::uint8_t if_conf     = 0;
    const int8_t aux_rslt    = bmi2_get_regs(BMI2_AUX_IF_CONF_ADDR, &aux_if_conf, 1, dev);
    const int8_t if_rslt     = bmi2_get_regs(BMI2_IF_CONF_ADDR, &if_conf, 1, dev);
    ESP_LOGI(kTag, "IMU_AUX aux_if_conf=0x%02X(rslt=%d) if_conf=0x%02X(rslt=%d) aux_en_bit=%u asda_pupsel=%u",
             aux_if_conf, static_cast<int>(aux_rslt), if_conf, static_cast<int>(if_rslt),
             static_cast<unsigned>((if_conf >> 5) & 0x01U), static_cast<unsigned>(if_conf & 0x03U));

    // The auxiliary connector is unpopulated, but AUX_IF_TRIM resets to 0x01, which
    // leaves a 40 kohms pull-up on ASDA. Datasheet pin notes: an unused secondary
    // interface must not be pulled to GND, and the internal pull-up is controlled by
    // asda_pupsel. Clear it so the floating ASDx/ASCx nets cannot conduct.
    std::uint8_t aux_if_trim = 0;
    if (bmi2_get_regs(BMI2_AUX_IF_TRIM, &aux_if_trim, 1, dev) == BMI2_OK) {
        std::uint8_t trimmed = static_cast<std::uint8_t>(aux_if_trim & ~0x03U);
        if (bmi2_set_regs(BMI2_AUX_IF_TRIM, &trimmed, 1, dev) == BMI2_OK) {
            bmi2_get_regs(BMI2_AUX_IF_TRIM, &aux_if_trim, 1, dev);
            ESP_LOGI(kTag, "IMU_AUX_TRIM asda_pupsel=0 verified=0x%02X", aux_if_trim);
        } else {
            ESP_LOGW(kTag, "IMU_AUX_TRIM write_failed");
        }
    } else {
        ESP_LOGW(kTag, "IMU_AUX_TRIM read_failed");
    }

    struct bmi2_int_pin_config pin_config = {};
    result                                = from_bmi(bmi2_get_int_pin_config(&pin_config, dev));
    if (!result.ok()) {
        return result;
    }
    pin_config.pin_type = BMI2_INT1;
    // Active low keeps the idle level at VDDIO. The wake input it drives carries a
    // pull-up, so an idle-low push-pull output would sink ~40 uA continuously.
    pin_config.pin_cfg[0].lvl       = BMI2_INT_ACTIVE_LOW;
    pin_config.pin_cfg[0].od        = BMI2_INT_PUSH_PULL;
    pin_config.pin_cfg[0].output_en = BMI2_INT_OUTPUT_ENABLE;
    pin_config.pin_cfg[0].input_en  = BMI2_INT_INPUT_DISABLE;
    // Active low and non-latched: the idle level is high, and asserting the
    // interrupt pulls the line low, which is the falling edge the PMIC G4 wake
    // input is armed on.
    pin_config.int_latch = BMI2_INT_NON_LATCH;
    result               = from_bmi(bmi2_set_int_pin_config(&pin_config, dev));
    if (!result.ok()) {
        return result;
    }

    struct bmi2_sens_int_config sens_int = {.type = BMI2_ANY_MOTION, .hw_int_pin = BMI2_INT1};
    result                               = from_bmi(bmi270_map_feat_int(&sens_int, 1, dev));
    if (!result.ok()) {
        return result;
    }
    g_any_motion = true;
    return {};
}

const char* accel_odr_label()
{
    return kAccelOdrLabel;
}

bool any_motion_configured()
{
    return g_any_motion;
}

Result read_wake_status(bool* any_motion)
{
    if (!g_initialized || any_motion == nullptr) {
        return {ESP_ERR_INVALID_ARG, 0};
    }
    struct bmi2_dev* dev = device();
    if (dev == nullptr) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    std::uint8_t status[2] = {};
    const Result result    = from_bmi(bmi2_get_regs(BMI2_INT_STATUS_0_ADDR, status, sizeof(status), dev));
    if (!result.ok()) {
        return result;
    }
    *any_motion = (status[0] & BMI270_ANY_MOT_STATUS_MASK) != 0U;
    return {};
}

Result clear_wake_status()
{
    bool any_motion = false;
    return read_wake_status(&any_motion);
}

Result start_sampling(std::uint32_t hz)
{
    if (!g_initialized || hz == 0) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    if (g_sample_task != nullptr) {
        return {};
    }
    struct bmi2_dev* dev = device();
    if (dev == nullptr) {
        return {ESP_ERR_INVALID_STATE, 0};
    }

    Result result = from_bmi(bmi2_set_adv_power_save(BMI2_DISABLE, dev));
    if (!result.ok()) {
        return result;
    }
    result = configure_sensor_load(dev, WakeProfile::reference);
    if (!result.ok()) {
        return result;
    }
    const std::uint8_t sensors[] = {BMI2_ACCEL, BMI2_GYRO};
    result                       = from_bmi(bmi270_sensor_enable(sensors, 2, dev));
    if (!result.ok()) {
        return result;
    }

    g_sample_period_ticks = pdMS_TO_TICKS(1000U / hz);
    if (g_sample_period_ticks == 0) {
        g_sample_period_ticks = 1;
    }
    if (xTaskCreate(sampling_task, "m5cc_imu", 4096, nullptr, 3, &g_sample_task) != pdPASS) {
        g_sample_task = nullptr;
        return {ESP_ERR_NO_MEM, 0};
    }
    return {};
}

void stop_sampling()
{
    if (g_sample_task == nullptr) {
        return;
    }
    vTaskDelete(g_sample_task);
    g_sample_task        = nullptr;
    struct bmi2_dev* dev = device();
    if (dev != nullptr) {
        const std::uint8_t sensors[] = {BMI2_ACCEL, BMI2_GYRO};
        bmi270_sensor_disable(sensors, 2, dev);
        bmi2_set_adv_power_save(BMI2_ENABLE, dev);
    }
}

std::uint32_t sample_count()
{
    return g_sample_count;
}

}  // namespace m5cc::imu
