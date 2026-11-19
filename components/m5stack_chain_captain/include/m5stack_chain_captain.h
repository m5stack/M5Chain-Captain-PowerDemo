#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_err.h"
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <driver/ledc.h>

#include "esp_lvgl_port.h"
#include "lvgl.h"

// Audio includes
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "audio_codec_if.h"

#ifdef __cplusplus
#include "M5IOE1.h"
#include "M5PM1.h"
extern "C" {
#endif


// 内部 I2C 配置
#define BSP_SYS_I2C_NUM         I2C_NUM_0
#define BSP_SYS_I2C_GPIO_SCL    GPIO_NUM_2
#define BSP_SYS_I2C_GPIO_SDA    GPIO_NUM_3

// LCD 配置
#define BSP_LCD_PCLK        GPIO_NUM_15
#define BSP_LCD_MOSI        GPIO_NUM_16
#define BSP_LCD_MISO        GPIO_NUM_NC
#define BSP_LCD_CS          GPIO_NUM_45
#define BSP_LCD_DC          GPIO_NUM_46
#define BSP_LCD_RST         GPIO_NUM_NC // Reset is controlled by M5IOE1 G1
#define BSP_LCD_RST_IOE     M5IOE1_PIN_1

// 按钮 GPIO 配置
#define BUTTON_1_GPIO       GPIO_NUM_1
#define BUTTON_2_GPIO       GPIO_NUM_5
#define BUTTON_3_GPIO       GPIO_NUM_4

// PMIC 中断引脚配置
#define BSP_PMIC_INT_GPIO   GPIO_NUM_1

// Audio Codec 配置
#define BSP_AUDIO_CODEC_I2C_NUM     I2C_NUM_1
#define BSP_AUDIO_I2C_SCL_GPIO      BSP_SYS_I2C_SCL_GPIO
#define BSP_AUDIO_I2C_SDA_GPIO      BSP_SYS_I2C_SCL_GPIO
#define BSP_AUDIO_CODEC_I2C_ADDR    ES8311_CODEC_DEFAULT_ADDR
#define BSP_AUDIO_I2S_GPIO_MCLK     GPIO_NUM_40
#define BSP_AUDIO_I2S_GPIO_BCLK     GPIO_NUM_38
#define BSP_AUDIO_I2S_GPIO_WS       GPIO_NUM_41
#define BSP_AUDIO_I2S_GPIO_DOUT     GPIO_NUM_42
#define BSP_AUDIO_I2S_GPIO_DIN      GPIO_NUM_39
#define BSP_AUDIO_CODEC_GPIO_PA     GPIO_NUM_21
#define BSP_AUDIO_CODEC_USE_MCLK    true
#define BSP_AUDIO_CODEC_PA_INVERTED false


void bsp_low_power_state1(void);


/**
 * @brief BSP display configuration structure
 *
 */
typedef struct {
    int max_transfer_sz;    /*!< Maximum transfer size, in bytes. */
} bsp_display_config_t;

//==============================================================================================
// BSP
//==============================================================================================
esp_err_t bsp_init(void);

#ifdef __cplusplus
/**
 * @brief Get M5IOE1 device handle
 * @return Pointer to M5IOE1 instance, or NULL if not initialized
 */
M5IOE1 *bsp_ioe_get_handle(void);

/**
 * @brief Get M5PM1 device handle
 * @return Pointer to M5PM1 instance, or NULL if not initialized
 * @note PM1 INT pin support: The current begin() overload does not support INT pin.
 *       If hardware interrupt is needed, check managed component API for INT pin support.
 */
M5PM1 *bsp_pmic_get_handle(void);
#endif

//==============================================================================================
// I2C
//==============================================================================================
typedef struct {
    uint8_t addresses[128];  // 找到的设备地址列表
    size_t count;            // 找到的设备数量
    bool scanned;            // 是否已扫描
} i2c_scan_result_t;

esp_err_t bsp_i2c_init(i2c_port_t i2c_num, gpio_num_t i2c_scl, gpio_num_t i2c_sda);
esp_err_t bsp_i2c_deinit(i2c_port_t i2c_num);
i2c_master_bus_handle_t bsp_i2c_get_handle(i2c_port_t i2c_num);
esp_err_t bsp_i2c_scan(i2c_port_t i2c_num, i2c_scan_result_t* result);

/**************************************************************************************************
 *
 * LCD interface
 *
 * M5Stack-Core is shipped with 2.0inch ILI9341C display controller.
 * It features 16-bit colors, 320x240 resolution.
 *
 * LVGL is used as graphics library. LVGL is NOT thread safe, therefore the user must take LVGL mutex
 * by calling bsp_display_lock() before calling and LVGL API (lv_...) and then give the mutex with
 * bsp_display_unlock().
 *
 * Display's backlight must be enabled explicitly by calling bsp_display_backlight_on()
 **************************************************************************************************/

/* LCD color formats */
#define ESP_LCD_COLOR_FORMAT_RGB565    (1)
#define ESP_LCD_COLOR_FORMAT_RGB888    (2)

/* LCD display color format */
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB565)
/* LCD display color bytes endianess */
#define BSP_LCD_BIGENDIAN           (1)
/* LCD display color bits */
#define BSP_LCD_BITS_PER_PIXEL      (16)
/* LCD display color space */
#define BSP_LCD_COLOR_SPACE         (ESP_LCD_COLOR_SPACE_BGR)
/* LCD definition */
#define BSP_LCD_H_RES              (240)
#define BSP_LCD_V_RES              (240)


#define BSP_LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)
#define BSP_LCD_SPI_NUM            (SPI2_HOST)
#define BSP_LCD_DRAW_BUFF_SIZE     (BSP_LCD_H_RES * 50)
#define BSP_LCD_DRAW_BUFF_DOUBLE   (1)

/**
 * @brief BSP display configuration structure
 */
typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg;  /*!< LVGL port configuration */
    uint32_t        buffer_size;    /*!< Size of the buffer for the screen in pixels */
    bool            double_buffer;  /*!< True, if should be allocated two buffers */
    struct {
        unsigned int buff_dma: 1;    /*!< Allocated LVGL buffer will be DMA capable */
        unsigned int buff_spiram: 1; /*!< Allocated LVGL buffer will be in PSRAM */
    } flags;
} bsp_display_cfg_t;

esp_err_t bsp_display_brightness_init(void);

esp_err_t bsp_display_brightness_set(int brightness_percent);

esp_err_t bsp_display_backlight_off(void);

esp_err_t bsp_display_backlight_on(void);

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @return Pointer to LVGL display or NULL when error occured
 */
lv_display_t *bsp_display_start(void);

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @param cfg display configuration
 *
 * @return Pointer to LVGL display or NULL when error occured
 */
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);

/**
 * @brief Get pointer to input device (touch, buttons, ...)
 *
 * @note The LVGL input device is initialized in bsp_display_start() function.
 *
 * @return Pointer to LVGL input device or NULL when not initialized
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in [ms]. 0 will block indefinitely.
 * @return true  Mutex was taken
 * @return false Mutex was NOT taken
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void bsp_display_unlock(void);

/**
 * @brief Rotate screen
 *
 * Display must be already initialized by calling bsp_display_start()
 *
 * @param[in] disp Pointer to LVGL display
 * @param[in] rotation Angle of the display rotation
 */
void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation);

//==============================================================================================
// Feature Enable/Disable
//==============================================================================================

/**
 * @brief Feature types that can be enabled/disabled
 */
typedef enum {
    BSP_FEATURE_AUDIO = 0,    /*!< Audio feature */
    BSP_FEATURE_LCD,           /*!< LCD display feature */
    BSP_FEATURE_GROVE,         /*!< Grove connector feature */
    BSP_FEATURE_IR,            /*!< Infrared feature */
    BSP_FEATURE_MAX            /*!< Maximum feature count */
} bsp_feature_t;

/**
 * @brief Enable or disable a specific feature
 *
 * This function configures the necessary GPIO pins and power settings
 * to enable or disable a specific feature on the M5Stack Chain Head.
 *
 * @param[in] feature Feature type to enable/disable
 * @param[in] enable true to enable, false to disable
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if feature is invalid
 *      - ESP_ERR_INVALID_STATE if required components are not initialized
 */
esp_err_t bsp_enable_feature(bsp_feature_t feature, bool enable);

//==============================================================================================
// Audio Functions
//==============================================================================================

/**
 * @brief PA Power Mode enumeration
 */
typedef enum {
    BSP_PA_MODE_1 = 1,    /*!< PA Mode 1 - 1 rising edge pulse */
    BSP_PA_MODE_2 = 2,    /*!< PA Mode 2 - 2 rising edge pulses (1W speaker) */
    BSP_PA_MODE_3 = 3,    /*!< PA Mode 3 - 3 rising edge pulses */
    BSP_PA_MODE_4 = 4,    /*!< PA Mode 4 - 4 rising edge pulses */
} bsp_pa_mode_t;

/**
 * @brief Initialize audio system
 *
 * @param[in] i2s_config I2S configuration (can be NULL for default)
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if configuration is invalid
 *      - ESP_ERR_NO_MEM if memory allocation failed
 */
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);

/**
 * @brief Initialize audio codec for speaker output
 *
 * @return Handle to the codec device, or NULL on failure
 */
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);

/**
 * @brief Initialize audio codec for microphone input
 *
 * @return Handle to the codec device, or NULL on failure
 */
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);

/**
 * @brief Read audio data from I2S
 *
 * @param[out] audio_buffer Buffer to store audio data
 * @param[in] len Length of data to read
 * @param[out] bytes_read Number of bytes actually read
 * @param[in] timeout_ms Timeout in milliseconds
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameters are invalid
 *      - ESP_ERR_TIMEOUT if operation timed out
 */
esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms);

/**
 * @brief Write audio data to I2S
 *
 * @param[in] audio_buffer Buffer containing audio data
 * @param[in] len Length of data to write
 * @param[out] bytes_written Number of bytes actually written
 * @param[in] timeout_ms Timeout in milliseconds
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameters are invalid
 *      - ESP_ERR_TIMEOUT if operation timed out
 */
esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);

/**
 * @brief Set codec sample rate and bit configuration
 *
 * @param[in] rate Sample rate in Hz
 * @param[in] bits_cfg Bit configuration
 * @param[in] ch Channel mode
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameters are invalid
 */
esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);

/**
 * @brief Set codec volume
 *
 * @param[in] volume Volume level
 * @param[out] volume_set Actual volume level set
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameters are invalid
 */
esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set);

/**
 * @brief Set codec mute state
 *
 * @param[in] enable true to mute, false to unmute
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameters are invalid
 */
esp_err_t bsp_extra_codec_mute_set(bool enable);

/**
 * @brief Stop codec device
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if device is not initialized
 */
esp_err_t bsp_extra_codec_dev_stop(void);

/**
 * @brief Resume codec device
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if device is not initialized
 */
esp_err_t bsp_extra_codec_dev_resume(void);

/**
 * @brief Initialize extra codec functionality
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if initialization failed
 */
esp_err_t bsp_extra_codec_init(void);

/**
 * @brief Set PA power mode using one-wire protocol
 *
 * This function controls the PA (Power Amplifier) power mode by sending
 * a specific number of rising edge pulses on the PA control pin.
 * 
 * Protocol:
 * 1. Keep PA pin low for more than 1ms
 * 2. Send 1-4 rising edge pulses based on mode (0.75us < TL, TH < 10us)
 * 
 * @param[in] mode PA power mode (1-4)
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if mode is invalid
 *      - ESP_ERR_INVALID_STATE if PA pin is not configured
 */
esp_err_t bsp_audio_set_pa_mode(bsp_pa_mode_t mode);

#ifdef __cplusplus
}
#endif
