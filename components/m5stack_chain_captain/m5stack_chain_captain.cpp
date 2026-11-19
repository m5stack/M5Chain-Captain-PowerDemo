/******************************************************************************
 * M5Stack Chain Captain BSP
 * @author: Enable
 * @date: 2025/10/24    
 * @version: 0.0.1
 * @description: Chain Captain BSP
 ******************************************************************************/
#include "m5stack_chain_captain.h"
#include "bsp_err_check.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <new>
#include <string.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>

#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <esp_timer.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"


static const char *TAG = "M5Stack Chain Captain";

// PA control pin state
static bool pa_pin_configured = false;

static lv_display_t *disp;
static i2c_master_bus_handle_t sys_i2c_bus = NULL;
static M5IOE1 _ioe_obj;
static M5PM1 _pmic_obj;
static M5IOE1 *ioe = NULL;
static M5PM1 *pmic = NULL;
static bool pmic_initialized = false;
static bool ioe_initialized = false;

/* NOTE: PM1 INT pin support
 * The managed M5PM1 component's begin(i2c_master_bus_handle_t, ...) overload
 * does not support INT pin parameter. If hardware interrupt is needed,
 * check if there's a begin() overload with intPin parameter or use
 * a different initialization method.
 */

esp_err_t bsp_init(void)
{
    ESP_LOGI(TAG, "BSP Init.");

    if (pmic != NULL) {
        _pmic_obj.~M5PM1();
        new (&_pmic_obj) M5PM1();
        pmic = NULL;
    }
    if (ioe != NULL) {
        _ioe_obj.~M5IOE1();
        new (&_ioe_obj) M5IOE1();
        ioe = NULL;
    }
    pmic_initialized = false;
    ioe_initialized = false;
    const esp_err_t i2c_result =
        bsp_i2c_init(BSP_SYS_I2C_NUM, BSP_SYS_I2C_GPIO_SCL, BSP_SYS_I2C_GPIO_SDA);
    if (i2c_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize system I2C: %s", esp_err_to_name(i2c_result));
        return i2c_result;
    }
    sys_i2c_bus = bsp_i2c_get_handle(BSP_SYS_I2C_NUM);
    if (sys_i2c_bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    pmic = &_pmic_obj;
    if (pmic->begin(sys_i2c_bus, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K) == M5PM1_OK) {
        pmic_initialized = true;
    } else {
        ESP_LOGE(TAG, "Failed to create m5stack pm1 device");
    }
    bool pmic_config_ok = pmic_initialized;
    bool single_reset_disabled = true;
    if (pmic_initialized &&
        pmic->setSingleResetDisable(false) != M5PM1_OK) {
        ESP_LOGE(TAG, "Failed to enable PMIC single-click reset");
        pmic_config_ok = false;
    } else if (pmic_initialized &&
               (pmic->getSingleResetDisable(&single_reset_disabled) != M5PM1_OK ||
                single_reset_disabled)) {
        ESP_LOGE(TAG, "PMIC single-click reset enable verification failed");
        pmic_config_ok = false;
    } else if (pmic_initialized) {
        ESP_LOGI(TAG, "PMIC single-click reset enabled and verified");
    }
    if (pmic_initialized && pmic->setI2cSleepTime(0) != M5PM1_OK) {
        ESP_LOGE(TAG, "Failed to disable PMIC I2C sleep");
        pmic_config_ok = false;
    }
    if (pmic_initialized && pmic->setChargeEnable(true) != M5PM1_OK) {
        ESP_LOGE(TAG, "Failed to enable charging during initialization");
        pmic_config_ok = false;
    }
    if (pmic_initialized && pmic->setLedEnLevel(true) != M5PM1_OK) {
        ESP_LOGE(TAG, "Failed to enable PMIC PWR_G_LED");
        pmic_config_ok = false;
    }
    if (pmic_initialized && pmic->setBoostEnable(false) != M5PM1_OK) {
        ESP_LOGE(TAG, "Failed to disable PMIC boost during initialization");
        pmic_config_ok = false;
    }

    ioe = &_ioe_obj;
    if (ioe->begin(sys_i2c_bus, 0x4F, M5IOE1_I2C_FREQ_100K,
                   M5IOE1_INT_MODE_POLLING) == M5IOE1_OK) {
        ioe_initialized = true;
        // The IO expander survives a host reset. Blank the retained LCD image
        // as soon as the expander is available, before the boot self-test.
        ioe->pinMode(M5IOE1_PIN_11, OUTPUT);
        ioe->setDriveMode(M5IOE1_PIN_11, M5IOE1_DRIVE_PUSHPULL);
        ioe->digitalWrite(M5IOE1_PIN_11, LOW);
    } else {
        ESP_LOGE(TAG, "Failed to create m5stack ioe device");
    }

    bool ir_default_off_ok = false;
    if (ioe_initialized) {
        ir_default_off_ok = bsp_enable_feature(BSP_FEATURE_IR, false) == ESP_OK;
        if (!ir_default_off_ok) {
            ESP_LOGE(TAG, "Failed to set IR power OFF during initialization");
        }
    }

    return pmic_config_ok && ioe_initialized && ir_default_off_ok ? ESP_OK : ESP_FAIL;
}

M5IOE1 *bsp_ioe_get_handle(void)
{
    return ioe_initialized ? ioe : NULL;
}

M5PM1 *bsp_pmic_get_handle(void)
{
    return pmic_initialized ? pmic : NULL;
}

//==============================================================================================
// I2C
//==============================================================================================
static bool i2c_initialized[2] = {false, false};
static i2c_master_bus_handle_t bsp_i2c_bus_handle[2] = {NULL, NULL};
static i2c_scan_result_t i2c_scan_result[2];

esp_err_t bsp_i2c_init(i2c_port_t i2c_num, gpio_num_t i2c_scl, gpio_num_t i2c_sda)
{
    if (i2c_num < I2C_NUM_0 || i2c_num > I2C_NUM_1) {
        return ESP_ERR_INVALID_ARG;
    }

    if (i2c_initialized[i2c_num]) {
        return ESP_OK;
    }

    i2c_master_bus_config_t i2c_bus_config = {};
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.i2c_port = i2c_num;
    i2c_bus_config.scl_io_num = i2c_scl;
    i2c_bus_config.sda_io_num = i2c_sda;
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = false;

    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &bsp_i2c_bus_handle[i2c_num]);
    if (ret != ESP_OK) {
        return ret;
    }

    i2c_initialized[i2c_num] = true;
    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(i2c_port_t i2c_num)
{
    if (i2c_num < I2C_NUM_0 || i2c_num > I2C_NUM_1) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!i2c_initialized[i2c_num]) {
        return ESP_OK;
    }

    esp_err_t ret = i2c_del_master_bus(bsp_i2c_bus_handle[i2c_num]);
    if (ret == ESP_OK) {
        bsp_i2c_bus_handle[i2c_num] = NULL;
        i2c_initialized[i2c_num] = false;
    }
    return ret;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(i2c_port_t i2c_num)
{
    if (i2c_num < I2C_NUM_0 || i2c_num > I2C_NUM_1) {
        return NULL;
    }
    return bsp_i2c_bus_handle[i2c_num];
}

esp_err_t bsp_i2c_scan(i2c_port_t i2c_num, i2c_scan_result_t* result)
{
    if (i2c_num < I2C_NUM_0 || i2c_num > I2C_NUM_1) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_bus_handle[i2c_num];
    if (bus == NULL) {
        ESP_LOGE(TAG, "I2C%d not initialized!\n", i2c_num);
        return ESP_ERR_INVALID_STATE;
    }
    memset(&i2c_scan_result[i2c_num], 0, sizeof(i2c_scan_result_t));

    esp_err_t ret;
    uint8_t address;
    size_t count = 0;

    ESP_LOGI(TAG, "Scanning I2C%d bus...", i2c_num);
    printf("\n     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");

    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            address = i + j;
            ret = i2c_master_probe(bus, address, 50);
            if (ret == ESP_OK) {
                printf("%02X ", address);
                if (count < 128) {
                    i2c_scan_result[i2c_num].addresses[count++] = address;
                }
            } else if (ret == ESP_ERR_TIMEOUT) {
                printf("UU ");
            } else {
                printf("-- ");
            }
        }
        printf("\r\n");
    }
    printf("\nI2C%d scan finished\n", i2c_num);
    
    i2c_scan_result[i2c_num].count = count;
    i2c_scan_result[i2c_num].scanned = true;
    if (result) {
        memcpy(result, &i2c_scan_result[i2c_num], sizeof(i2c_scan_result_t));
    }
    
    ESP_LOGI(TAG, "I2C%d scan found %d devices", i2c_num, count);
    return ESP_OK;
}

//==============================================================================================
// LCD
//==============================================================================================
static bool spi_initialized = false;

typedef struct {
    uint8_t command;
    uint8_t data[32];
    uint8_t data_size;
    uint16_t delay_ms;
} jd9853_init_cmd_t;

static esp_err_t bsp_jd9853_init(esp_lcd_panel_io_handle_t io)
{
    static const jd9853_init_cmd_t init_commands[] = {
        {0x01, {}, 0, 20},
        {0xDF, {0x98, 0x53}, 2, 0},
        {0xDE, {0x00}, 1, 0},
        {0xB2, {0x38}, 1, 0},
        {0xB7, {0x00, 0x29, 0x00, 0x51}, 4, 0},
        {0xBB, {0x28, 0x2F, 0x55, 0x73, 0x63, 0xF0}, 6, 0},
        {0xC0, {0x22, 0xA2}, 2, 0},
        {0xC1, {0x12}, 1, 0},
        {0xC3, {0x7D, 0x08, 0x08, 0x0C, 0xC2, 0x72, 0x22, 0x88}, 8, 0},
        {0xC4, {0x00, 0x00, 0xA0, 0x79, 0x0A, 0x0B, 0x16, 0x79,
                0x0A, 0x0B, 0x16, 0x82}, 12, 0},
        {0xC8, {0x3F, 0x29, 0x23, 0x20, 0x27, 0x2D, 0x2A, 0x2B,
                0x2A, 0x28, 0x25, 0x17, 0x12, 0x0D, 0x07, 0x02,
                0x3F, 0x29, 0x23, 0x20, 0x27, 0x2D, 0x2A, 0x2B,
                0x2A, 0x28, 0x25, 0x17, 0x12, 0x0D, 0x07, 0x02}, 32, 0},
        {0xD0, {0x04, 0x04, 0x6A, 0x1C, 0x03}, 5, 0},
        {0xD7, {0x00, 0x20}, 2, 0},
        {0xE6, {0x10}, 1, 0},
        {0xDE, {0x01}, 1, 0},
        {0xBB, {0x04}, 1, 0},
        {0xD7, {0x12}, 1, 0},
        {0xB7, {0x03, 0x13, 0xE5, 0x38, 0x38}, 5, 0},
        {0xC1, {0x14, 0x15, 0xC0}, 3, 0},
        {0xC2, {0x06, 0x3A}, 2, 0},
        {0xC4, {0x72, 0x12}, 2, 0},
        {0xBE, {0x00}, 1, 0},
        {0xDE, {0x00}, 1, 0},
        {0x35, {0x00}, 1, 0},
        {0x36, {0x00}, 1, 0},
        {0x3A, {0x05}, 1, 0},
        {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4, 0},
        {0x2B, {0x00, 0x00, 0x00, 0xEF}, 4, 0},
        {0x11, {}, 0, 120},
        {0x29, {}, 0, 10},
    };

    for (const auto &entry : init_commands) {
        ESP_RETURN_ON_ERROR(
            esp_lcd_panel_io_tx_param(io, entry.command,
                                      entry.data_size ? entry.data : nullptr,
                                      entry.data_size),
            TAG, "JD9853 init command 0x%02X failed", entry.command);
        if (entry.delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(entry.delay_ms));
        }
    }
    return ESP_OK;
}

static esp_err_t bsp_spi_init(uint32_t max_transfer_sz)
{
    /* SPI was initialized before */
    if (spi_initialized) {
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = BSP_LCD_PCLK;
    buscfg.mosi_io_num = BSP_LCD_MOSI;
    buscfg.miso_io_num = BSP_LCD_MISO;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = max_transfer_sz;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    spi_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_display_brightness_init(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(bsp_enable_feature(BSP_FEATURE_LCD, true));

    ioe->pinMode(M5IOE1_PIN_11, OUTPUT);
    ioe->setDriveMode(M5IOE1_PIN_11, M5IOE1_DRIVE_PUSHPULL);
    ioe->digitalWrite(M5IOE1_PIN_11, LOW);

    // M5IOE1 uses one frequency for every PWM channel. Configure it once
    // above the audible range for the LCD backlight and RGB channels.
    constexpr uint16_t pwm_frequency_hz = 20000;
    const m5ioe1_err_t pwm_result = ioe->setPwmFrequency(pwm_frequency_hz);
    if (pwm_result != M5IOE1_OK) {
        ESP_LOGE(TAG, "Failed to configure shared M5IOE1 PWM frequency: %d", pwm_result);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "M5IOE1 shared PWM frequency verified: %u Hz",
             static_cast<unsigned>(pwm_frequency_hz));

    const m5ioe1_err_t duty_result = ioe->setPwmDuty(M5IOE1_PWM_CH3, 0);
    if (duty_result != M5IOE1_OK) {
        ESP_LOGE(TAG, "Failed to blank LCD backlight: %d", duty_result);
        return ESP_FAIL;
    }
    ioe->digitalWrite(M5IOE1_PIN_11, HIGH);

    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ioe->setPwmDuty(M5IOE1_PWM_CH3, brightness_percent);

    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    assert(config != NULL && config->max_transfer_sz > 0);

    BSP_ERROR_CHECK_RETURN_ERR(bsp_enable_feature(BSP_FEATURE_LCD, true));

    // Force a full LCD power cycle. The LCD and IO expander remain powered
    // across a host-only reset, so a retained frame otherwise stays visible.
    ioe->pinMode(M5IOE1_PIN_12, OUTPUT);
    ioe->setDriveMode(M5IOE1_PIN_12, M5IOE1_DRIVE_PUSHPULL);
    ioe->pinMode(BSP_LCD_RST_IOE, OUTPUT);
    ioe->setDriveMode(BSP_LCD_RST_IOE, M5IOE1_DRIVE_PUSHPULL);
    ioe->digitalWrite(M5IOE1_PIN_12, LOW);
    ioe->digitalWrite(BSP_LCD_RST_IOE, LOW);
    vTaskDelay(pdMS_TO_TICKS(20));
    ioe->digitalWrite(M5IOE1_PIN_12, HIGH);
    vTaskDelay(pdMS_TO_TICKS(20));
    ioe->digitalWrite(BSP_LCD_RST_IOE, HIGH);
    vTaskDelay(pdMS_TO_TICKS(120));
    /* Initialize SPI */
    ESP_RETURN_ON_ERROR(bsp_spi_init(config->max_transfer_sz), TAG, "");

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {};
    esp_lcd_panel_dev_config_t panel_config = {};
    
    io_config.dc_gpio_num = BSP_LCD_DC;
    io_config.cs_gpio_num = BSP_LCD_CS;
    io_config.pclk_hz = 40*1000*1000;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, ret_io), err, TAG,
                      "New panel IO failed");

    ESP_LOGI(TAG, "Install LCD driver");
    panel_config.reset_gpio_num = BSP_LCD_RST;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(*ret_io, &panel_config, ret_panel), err, TAG, "New panel failed");
    // ESP_GOTO_ON_ERROR(esp_lcd_new_panel_ili9341(*ret_io, &panel_config, ret_panel), err, TAG, "New panel failed");

    // JD9853 uses the ST7789-compatible CASET/PASET/RAMWR drawing path, but
    // requires the module vendor's own power, gamma and timing setup.
    ESP_GOTO_ON_ERROR(bsp_jd9853_init(*ret_io), err, TAG, "JD9853 panel init failed");
    esp_lcd_panel_set_gap(*ret_panel, 0, 80);
    ESP_GOTO_ON_ERROR(esp_lcd_panel_mirror(*ret_panel, true, true), err, TAG,
                      "JD9853 mirror setup failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_invert_color(*ret_panel, false), err, TAG,
                      "JD9853 color inversion setup failed");

    return ret;
err:
    if (*ret_panel) {
        esp_lcd_panel_del(*ret_panel);
    }
    if (*ret_io) {
        esp_lcd_panel_io_del(*ret_io);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);
    return ret;
}

static lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = NULL;
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = BSP_LCD_DRAW_BUFF_SIZE * sizeof(uint16_t),
    };

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle));

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.io_handle = io_handle;
    disp_cfg.panel_handle = panel_handle;
    disp_cfg.buffer_size = cfg->buffer_size;
    disp_cfg.double_buffer = cfg->double_buffer;
    disp_cfg.hres = BSP_LCD_H_RES;
    disp_cfg.vres = BSP_LCD_V_RES;
    disp_cfg.monochrome = false;
    /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
    disp_cfg.rotation.swap_xy = false;
    disp_cfg.rotation.mirror_x = true;
    disp_cfg.rotation.mirror_y = true;
    disp_cfg.flags.buff_dma = cfg->flags.buff_dma;
    disp_cfg.flags.buff_spiram = cfg->flags.buff_spiram;
#if LVGL_VERSION_MAJOR >= 9
    disp_cfg.flags.swap_bytes = (BSP_LCD_BIGENDIAN ? true : false);
#endif

    return lvgl_port_add_disp(&disp_cfg);
}

lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size   = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        }                                  
    };
    cfg.lvgl_port_cfg.task_affinity = 1; /* For camera */
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_init(&cfg->lvgl_port_cfg));

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_brightness_init());

    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);

    // Turn on backlight
    // bsp_display_backlight_on();

    return disp;
}

void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation)
{
    lv_disp_set_rotation(disp, rotation);
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}

//==============================================================================================
// Button
//==============================================================================================
// esp_err_t button_init(void)
// {
//     // Button 1 configuration
//     const button_config_t btn1_cfg = {
//         .long_press_time = 2000,
//         .short_press_time = 0,
//     };
    
//     const button_gpio_config_t btn1_gpio_cfg = {
//         .gpio_num = BUTTON_1_GPIO,
//         .active_level = 0,
//         .enable_power_save = false,
//         .disable_pull = false,
//     };
    
//     // Button 2 configuration
//     const button_config_t btn2_cfg = {
//         .long_press_time = 2000,
//         .short_press_time = 0,
//     };
    
//     const button_gpio_config_t btn2_gpio_cfg = {
//         .gpio_num = BUTTON_2_GPIO,
//         .active_level = 0,
//         .enable_power_save = false,
//         .disable_pull = false,
//     };

//     // Button 3 configuration
//     const button_config_t btn3_cfg = {
//         .long_press_time = 2000,
//         .short_press_time = 0,
//     };
    
//     const button_gpio_config_t btn3_gpio_cfg = {
//         .gpio_num = BUTTON_3_GPIO,
//         .active_level = 0,
//         .enable_power_save = false,
//         .disable_pull = false,
//     };
    
//     // Create button handles
//     esp_err_t ret1 = iot_button_new_gpio_device(&btn1_cfg, &btn1_gpio_cfg, &btn1_handle);
//     esp_err_t ret2 = iot_button_new_gpio_device(&btn2_cfg, &btn2_gpio_cfg, &btn2_handle);   
//     esp_err_t ret3 = iot_button_new_gpio_device(&btn3_cfg, &btn3_gpio_cfg, &btn3_handle);
    
//     if (ret1 != ESP_OK || ret2 != ESP_OK || ret3 != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to create button handles");
//         return ESP_FAIL;
//     }   
    
//     // 注册按钮回调，传递按钮句柄作为参数
//     iot_button_register_cb(btn1_handle, BUTTON_SINGLE_CLICK, NULL, button_click_cb, (void*)btn1_handle);
//     iot_button_register_cb(btn2_handle, BUTTON_SINGLE_CLICK, NULL, button_click_cb, (void*)btn2_handle);
//     iot_button_register_cb(btn3_handle, BUTTON_SINGLE_CLICK, NULL, button_click_cb, (void*)btn3_handle);

//     return ESP_OK;
// }

//==============================================================================================
// Feature Enable/Disable
//==============================================================================================
esp_err_t bsp_enable_feature(bsp_feature_t feature, bool enable)
{
    esp_err_t ret = ESP_OK;

    switch (feature) {
        case BSP_FEATURE_AUDIO:
            ESP_LOGI(TAG, "%s audio feature", enable ? "Enabling" : "Disabling");
            ioe->pinMode(M5IOE1_PIN_5, OUTPUT);
            ioe->setDriveMode(M5IOE1_PIN_5, M5IOE1_DRIVE_PUSHPULL);
            ioe->digitalWrite(M5IOE1_PIN_5, enable ? HIGH : LOW);
            break;

        case BSP_FEATURE_LCD:
            ESP_LOGI(TAG, "%s LCD feature", enable ? "Enabling" : "Disabling");
            ioe->pinMode(M5IOE1_PIN_12, OUTPUT);
            ioe->setDriveMode(M5IOE1_PIN_12, M5IOE1_DRIVE_PUSHPULL);
            ioe->digitalWrite(M5IOE1_PIN_12, enable ? HIGH : LOW);
            break;

        case BSP_FEATURE_GROVE:
            ESP_LOGI(TAG, "%s Grove feature", enable ? "Enabling" : "Disabling");
            pmic->pinMode(3, OUTPUT);
            pmic->digitalWrite(3, enable ? HIGH : LOW);
            pmic->setBoostEnable(enable);
            break;

        case BSP_FEATURE_IR:
            ESP_LOGI(TAG, "%s IR feature", enable ? "Enabling" : "Disabling");
            if (ioe == NULL) {
                ESP_LOGE(TAG, "M5IOE1 is not initialized");
                return ESP_ERR_INVALID_STATE;
            }
            {
                m5ioe1_err_t ioe_ret = M5IOE1_OK;
                ioe->pinModeWithRes(M5IOE1_PIN_2, OUTPUT, &ioe_ret);
                if (ioe_ret != M5IOE1_OK) {
                    ESP_LOGE(TAG, "Failed to configure IOEG2_IR_L3B_EN: %d", ioe_ret);
                    return ESP_FAIL;
                }
                ioe_ret = ioe->setDriveMode(M5IOE1_PIN_2, M5IOE1_DRIVE_PUSHPULL);
                if (ioe_ret != M5IOE1_OK) {
                    ESP_LOGE(TAG, "Failed to set IOEG2_IR_L3B_EN drive mode: %d", ioe_ret);
                    return ESP_FAIL;
                }
                ioe->digitalWriteWithRes(M5IOE1_PIN_2, enable ? HIGH : LOW, &ioe_ret);
                if (ioe_ret != M5IOE1_OK) {
                    ESP_LOGE(TAG, "Failed to set IOEG2_IR_L3B_EN level: %d", ioe_ret);
                    return ESP_FAIL;
                }
                const int expected_level = enable ? HIGH : LOW;
                const int actual_level = ioe->digitalReadWithRes(M5IOE1_PIN_2, &ioe_ret);
                if (ioe_ret != M5IOE1_OK || actual_level != expected_level) {
                    ESP_LOGE(TAG,
                             "IOEG2_IR_L3B_EN verify failed: expected=%d actual=%d error=%d",
                             expected_level, actual_level, ioe_ret);
                    return ESP_FAIL;
                }
                ESP_LOGI(TAG, "IOEG2_IR_L3B_EN verified: %s level=%d",
                         enable ? "ON" : "OFF", actual_level);
            }
            break;
            
        default:
            ESP_LOGE(TAG, "Invalid feature: %d", feature);
            return ESP_ERR_INVALID_ARG;
    }
    
    return ret;
}


//==============================================================================================
// Audio Codec
//==============================================================================================
static const audio_codec_data_if_t *i2s_data_if = NULL;  /* Codec data interface */
static i2s_chan_handle_t i2s_tx_chan = NULL;
static i2s_chan_handle_t i2s_rx_chan = NULL;
#define BSP_I2S_NUM I2S_NUM_0

#define BSP_I2S_GPIO_CFG       \
    {                          \
        .mclk = BSP_AUDIO_I2S_GPIO_MCLK,  \
        .bclk = BSP_AUDIO_I2S_GPIO_BCLK,  \
        .ws = BSP_AUDIO_I2S_GPIO_WS,    \
        .dout = BSP_AUDIO_I2S_GPIO_DOUT,  \
        .din = BSP_AUDIO_I2S_GPIO_DIN,   \
        .invert_flags = {      \
            .mclk_inv = false, \
            .bclk_inv = false, \
            .ws_inv = false,   \
        },                     \
    }

#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }


esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    esp_err_t ret = ESP_FAIL;

    bsp_enable_feature(BSP_FEATURE_AUDIO, true);
    // ？？？
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to enable audio feature");
    //     return ret;
    // }

    // PA Enable - Configure PA pin as output
    gpio_config_t pa_io_conf = {
        .pin_bit_mask = (1ULL << BSP_AUDIO_CODEC_GPIO_PA),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pa_io_conf);
    gpio_set_level(BSP_AUDIO_CODEC_GPIO_PA, 1);
    pa_pin_configured = true;

    // 设置PA模式为Mode 2（1W扬声器推荐模式）
    esp_err_t pa_ret = bsp_audio_set_pa_mode(BSP_PA_MODE_4);
    if (pa_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set PA mode 2: %s", esp_err_to_name(pa_ret));
    } else {
        ESP_LOGI(TAG, "PA mode 2 set for 1W speaker");
    }

    if (i2s_tx_chan && i2s_rx_chan) {
        /* Audio was initialized before */
        return ESP_OK;
    }

    /* Setup I2S peripheral */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    BSP_ERROR_CHECK_RETURN_ERR(i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan));

    /* Setup I2S channels */
    const i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(22050);
    const i2s_std_config_t *p_i2s_cfg = &std_cfg_default;
    if (i2s_config != NULL) {
        p_i2s_cfg = i2s_config;
    }

    audio_codec_i2s_cfg_t i2s_cfg = {};
    
    if (i2s_tx_chan != NULL) {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_chan, p_i2s_cfg), err, TAG, "I2S channel initialization failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_tx_chan), err, TAG, "I2S enabling failed");
    }
    if (i2s_rx_chan != NULL) {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_rx_chan, p_i2s_cfg), err, TAG, "I2S channel initialization failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_rx_chan), err, TAG, "I2S enabling failed");
    }

    i2s_cfg.port = BSP_I2S_NUM;
    i2s_cfg.tx_handle = i2s_tx_chan;
    i2s_cfg.rx_handle = i2s_rx_chan;
    i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    BSP_NULL_CHECK_GOTO(i2s_data_if, err);

    // xTaskCreate(i2s_example_write_task, "i2s_example_write_task", 4096, NULL, 5, NULL);

    return ESP_OK;

err:
    if (i2s_tx_chan) {
        i2s_del_channel(i2s_tx_chan);
    }
    if (i2s_rx_chan) {
        i2s_del_channel(i2s_rx_chan);
    }

    return ret;
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    if (i2s_data_if == NULL) {
        /* Initialize I2C */
        BSP_ERROR_CHECK_RETURN_NULL(bsp_i2c_init(BSP_SYS_I2C_NUM, BSP_SYS_I2C_GPIO_SCL, BSP_SYS_I2C_GPIO_SDA));
        /* Configure I2S peripheral and Power Amplifier */
        BSP_ERROR_CHECK_RETURN_NULL(bsp_audio_init(NULL));
    }
    assert(i2s_data_if);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    audio_codec_i2c_cfg_t i2c_cfg = {};
    i2c_cfg.port = BSP_SYS_I2C_NUM;
    i2c_cfg.addr = ES8311_CODEC_DEFAULT_ADDR;
    i2c_cfg.bus_handle = bsp_i2c_get_handle(BSP_SYS_I2C_NUM);
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    BSP_NULL_CHECK(i2c_ctrl_if, NULL);

    esp_codec_dev_hw_gain_t gain = {};
    gain.pa_voltage = 5.0;
    gain.codec_dac_voltage = 3.3;

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = i2c_ctrl_if;
    es8311_cfg.gpio_if = gpio_if;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8311_cfg.pa_pin = BSP_AUDIO_CODEC_GPIO_PA;
    es8311_cfg.pa_reverted = BSP_AUDIO_CODEC_PA_INVERTED;
    es8311_cfg.master_mode = false;
    es8311_cfg.use_mclk = BSP_AUDIO_CODEC_USE_MCLK;
    es8311_cfg.digital_mic = false;
    es8311_cfg.invert_mclk = false;
    es8311_cfg.invert_sclk = false;
    es8311_cfg.hw_gain = gain;
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    BSP_NULL_CHECK(es8311_dev, NULL);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_dev_cfg);
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    if (i2s_data_if == NULL) {
        /* Initialize I2C */
        BSP_ERROR_CHECK_RETURN_NULL(bsp_i2c_init(BSP_SYS_I2C_NUM, BSP_SYS_I2C_GPIO_SCL, BSP_SYS_I2C_GPIO_SDA));
        /* Configure I2S peripheral and Power Amplifier */
        BSP_ERROR_CHECK_RETURN_NULL(bsp_audio_init(NULL));
    }
    assert(i2s_data_if);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    audio_codec_i2c_cfg_t i2c_cfg = {};
    i2c_cfg.port = BSP_SYS_I2C_NUM;
    i2c_cfg.addr = ES8311_CODEC_DEFAULT_ADDR;
    i2c_cfg.bus_handle = bsp_i2c_get_handle(BSP_SYS_I2C_NUM);
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    BSP_NULL_CHECK(i2c_ctrl_if, NULL);

    esp_codec_dev_hw_gain_t gain = {};
    gain.pa_voltage = 5.0;
    gain.codec_dac_voltage = 3.3;

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = i2c_ctrl_if;
    es8311_cfg.gpio_if = gpio_if;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8311_cfg.pa_pin = BSP_AUDIO_CODEC_GPIO_PA;
    es8311_cfg.pa_reverted = BSP_AUDIO_CODEC_PA_INVERTED;
    es8311_cfg.master_mode = false;
    es8311_cfg.use_mclk = BSP_AUDIO_CODEC_USE_MCLK;
    es8311_cfg.digital_mic = false;
    es8311_cfg.invert_mclk = false;
    es8311_cfg.invert_sclk = false;
    es8311_cfg.hw_gain = gain;

    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    BSP_NULL_CHECK(es8311_dev, NULL);

    esp_codec_dev_cfg_t codec_es8311_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_es8311_dev_cfg);
}

//==============================================================================================
#define CODEC_DEFAULT_SAMPLE_RATE          (22050)
#define CODEC_DEFAULT_BIT_WIDTH            16
#define CODEC_DEFAULT_ADC_VOLUME           18.0
#define CODEC_DEFAULT_CHANNEL              I2S_SLOT_MODE_STEREO
static esp_codec_dev_handle_t play_dev_handle;
static esp_codec_dev_handle_t record_dev_handle;

esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_read(record_dev_handle, audio_buffer, len);
    *bytes_read = len;
    return ret;
}

esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_write(play_dev_handle, audio_buffer, len);
    *bytes_written = len;
    return ret;
}

esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_codec_dev_sample_info_t fs = {};
    fs.sample_rate = rate;
    fs.bits_per_sample = bits_cfg;
    fs.channel = ch;

    if (play_dev_handle == nullptr || record_dev_handle == nullptr ||
        play_dev_handle != record_dev_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_codec_dev_close(play_dev_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_codec_dev_open(play_dev_handle, &fs);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_codec_dev_set_in_gain(record_dev_handle, CODEC_DEFAULT_ADC_VOLUME);
}

esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set)
{
    if (play_dev_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_codec_dev_set_out_vol(play_dev_handle, volume);
    if (ret == ESP_OK && volume_set != nullptr) {
        ret = esp_codec_dev_get_out_vol(play_dev_handle, volume_set);
    }
    return ret;
}

esp_err_t bsp_extra_codec_mute_set(bool enable)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_set_out_mute(play_dev_handle, enable);
    return ret;
}

esp_err_t bsp_extra_codec_dev_stop(void)
{
    return play_dev_handle != nullptr ? esp_codec_dev_close(play_dev_handle) :
                                        ESP_ERR_INVALID_STATE;
}

esp_err_t bsp_extra_codec_dev_resume(void)
{
    return bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
}

esp_err_t bsp_extra_codec_init()
{
    if (play_dev_handle && record_dev_handle) {
        return bsp_extra_codec_dev_resume();
    }

    play_dev_handle = bsp_audio_codec_speaker_init();
    if (play_dev_handle == NULL) {
        return ESP_FAIL;
    }
    record_dev_handle = play_dev_handle;

    return bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH,
                                  CODEC_DEFAULT_CHANNEL);
}

//==============================================================================================
// PA Mode Control
//==============================================================================================
esp_err_t bsp_audio_set_pa_mode(bsp_pa_mode_t mode)
{
    if (mode < BSP_PA_MODE_1 || mode > BSP_PA_MODE_4) {
        ESP_LOGE(TAG, "Invalid PA mode: %d", mode);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!pa_pin_configured) {
        ESP_LOGE(TAG, "PA pin not configured");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Setting PA mode to %d", mode);
    
    // Step 1: Keep PA pin low for more than 1ms
    gpio_set_level(BSP_AUDIO_CODEC_GPIO_PA, 0);
    vTaskDelay(pdMS_TO_TICKS(2)); // 2ms delay to ensure >1ms
    
    // Step 2: Send the required number of rising edge pulses
    // Each pulse: 0.75us < TL, TH < 10us
    for (int i = 0; i < mode; i++) {
        // Rising edge: Low -> High -> Low
        gpio_set_level(BSP_AUDIO_CODEC_GPIO_PA, 1);
        esp_rom_delay_us(5); // 5us high time (within 0.75us < TH < 10us)
        gpio_set_level(BSP_AUDIO_CODEC_GPIO_PA, 0);
        esp_rom_delay_us(5); // 5us low time (within 0.75us < TL < 10us)
    }
    
    // Final state: keep PA pin low
    gpio_set_level(BSP_AUDIO_CODEC_GPIO_PA, 0);
    
    ESP_LOGI(TAG, "PA mode %d set successfully", mode);
    return ESP_OK;
}
