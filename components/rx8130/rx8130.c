/******************************************************************************
 * RTC RX8130CE:B3 Driver
 * @author: Enable
 * @date: 2025/10/24
 * 
 * 
 * 
 * 
 * 
 ******************************************************************************/
#include "rx8130.h"
#include "esp_log.h" 
#include "esp_check.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include <string.h>
#include <stdio.h>
#include "driver/gpio.h"



static const char *TAG = "rx8130";


static uint8_t bcd2dec(uint8_t bcd);
static uint8_t dec2bcd(uint8_t dec);
static esp_err_t write_regs(uint8_t reg, uint8_t *data, size_t len);
static esp_err_t read_regs(uint8_t reg, uint8_t *data, size_t len);

#define I2C_ADDR_RX8130 0x32
static i2c_master_dev_handle_t i2c_dev;
static gpio_num_t rx8130_int_pin = GPIO_NUM_NC;

/**
 * 初始化 RX8130 
 * @param i2c_bus I2C 总线句柄
 * @param irq     可选中断引脚（GPIO_NUM_NC 表示未连接）
 * @return 返回 ESP_OK 表示成功
 */
esp_err_t rx8130_init(i2c_master_bus_handle_t i2c_bus, gpio_num_t int_pin)
{
    ESP_LOGI(TAG, "Initializing RX8130 RTC...");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_ADDR_RX8130,
        .scl_speed_hz = 400000, // 400kHz
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
    if (ret == ESP_OK) {
        if (int_pin > GPIO_NUM_NC) {
            rx8130_int_pin = int_pin;
            gpio_reset_pin(rx8130_int_pin);
            gpio_set_direction(rx8130_int_pin, GPIO_MODE_INPUT);
            gpio_pullup_en(rx8130_int_pin);
            gpio_pulldown_dis(rx8130_int_pin);
        }
    }

    return ret;
}

/**
 * 注销 RX8130 设备并释放资源
 * @return 返回 ESP_OK 表示成功
 */
esp_err_t rx8130_deinit(void)
{
    if (i2c_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2c_master_bus_rm_device(i2c_dev);
    if (ret == ESP_OK) {
        i2c_dev = NULL;
    }

    // if (bm8563_int_pin > GPIO_NUM_NC) {
    //     /* Remove GPIO ISR handler */
    //     ret = gpio_isr_handler_remove(bm8563_int_pin);
    //     ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR remove handler failed");
    //     ret = gpio_intr_disable(bm8563_int_pin);
    //     ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR disable failed");
    //     ret = gpio_reset_pin(bm8563_int_pin);
    // }

    return ret;
}

/**
 * @brief Touch controller interrupt callback type
 *
 */
static rx8130_irq_cb_t rx8130_irq_cb = NULL;

static void IRAM_ATTR rx8130_gpio_isr_handler(void *arg)
{
   // ESP_LOGI(TAG, "rx8130_gpio_isr_handler");
    if (rx8130_irq_cb) {
        rx8130_irq_cb();
    }
}

esp_err_t rx8130_set_irq_callback(rx8130_irq_cb_t cb)
{
    esp_err_t ret = ESP_OK;

    if (rx8130_int_pin == GPIO_NUM_NC) {
        ESP_LOGE(TAG, "IRQ pin not configured");
        return ESP_ERR_INVALID_ARG;
    } 

    rx8130_irq_cb = cb;
    if (cb != NULL) {
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_NEGEDGE,           // 下降沿触发（或者 GPIO_INTR_LOW_LEVEL）
            .mode = GPIO_MODE_INPUT,                  // 输入模式
            .pin_bit_mask = (1ULL << rx8130_int_pin), // 引脚选择 
            .pull_up_en = GPIO_PULLUP_ENABLE,         // 上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE     // 不下拉
        };
        gpio_config(&io_conf);

        ESP_LOGI(TAG, "Installing GPIO ISR handler for RX8130 IRQ pin %d", rx8130_int_pin);
        ret = gpio_install_isr_service(0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "GPIO ISR install failed");
            return ret;
        }
        // ret = gpio_intr_enable(bm8563_int_pin);

        // 注册中断回调
        ret = gpio_isr_handler_add(rx8130_int_pin, rx8130_gpio_isr_handler, NULL);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR install failed");
    }  else {
        ESP_LOGI(TAG, "Removing GPIO ISR handler for RX8130 IRQ pin %d", rx8130_int_pin);
        /* Remove GPIO ISR handler */
        ret = gpio_isr_handler_remove(rx8130_int_pin);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR remove handler failed");
        ret = gpio_intr_disable(rx8130_int_pin);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR disable failed");
    }

    return ESP_OK;
}


/**
 * 设置时间
 * @param time 指向标准 tm 结构体的指针
 * @return 返回 ESP_OK 表示成功，其他表示失败
 */
esp_err_t rx8130_set_time(const struct tm *time)
{
    if (!time) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[7];
    buf[0] = dec2bcd(time->tm_sec)   & 0x7F;   // 秒，最高位保留为 0（VL 位）  VL(Voltage Low, 低电压检测位)
    buf[1] = dec2bcd(time->tm_min)   & 0x7F;   // 分
    buf[2] = dec2bcd(time->tm_hour)  & 0x3F;   // 时
    buf[3] = dec2bcd(time->tm_wday)  & 0x07;   // 星期（0 = 周日）
    buf[4] = dec2bcd(time->tm_mday)  & 0x3F;   // 日
    buf[5] = dec2bcd(time->tm_mon + 1) & 0x1F; // 月（tm_mon 范围是 0~11）
    buf[6] = dec2bcd(time->tm_year % 100);     // 年的后两位 (00-99)

    // 设置世纪位：2000年以后设置世纪位
    int full_year = time->tm_year + 1900;
    if (full_year >= 2000) {
        buf[6] |= 0x80;  // 在YEAR寄存器设置世纪位
    }

    return write_regs(REG_SEC, buf, 7);  
}

/**
 * 读取时间
 * @param time 指向 struct tm 结构体的指针，用于存储结果
 * @return 返回 ESP_OK 表示成功，其他错误代码表示失败
 */
esp_err_t rx8130_get_time(struct tm *time)
{
    if (!time) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[7] = {0};
    esp_err_t ret = read_regs(REG_SEC, buf, 7);  
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read time from RX8130: %s", esp_err_to_name(ret));
        return ret;
    }

    time->tm_sec  = bcd2dec(buf[0] & 0x7F);     // 秒，忽略 VL 位
    time->tm_min  = bcd2dec(buf[1] & 0x7F);     // 分
    time->tm_hour = bcd2dec(buf[2] & 0x3F);     // 时
    time->tm_wday = bcd2dec(buf[3] & 0x07);     // 星期
    time->tm_mday = bcd2dec(buf[4] & 0x3F);     // 日
    time->tm_mon  = bcd2dec(buf[5] & 0x1F) - 1; // 月（0–11）
    
    // 处理年份：RX8130存储年份的后两位(00-99)，需要根据世纪位确定完整年份
    uint8_t year_low = bcd2dec(buf[6] & 0x7F);  // 年份后两位 (00-99)，忽略世纪位
    
    if (buf[6] & 0x80) {
        // 世纪位为1，表示2000年以后 (2000-2099)
        time->tm_year = year_low + 100;  // 2000年 = 100（1900起算）
    } else {
        // 世纪位为0，表示1900-1999年
        time->tm_year = year_low;  // 1900-1999年
    }

    // 清除其他字段
    time->tm_yday = 0;
    time->tm_isdst = 0;
 
    return ESP_OK;
}

/**
 * 打印时间
 */
void rx8130_print_time(void)
{
    static const char *wday_str[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };

    struct tm t;
    if (rx8130_get_time(&t) == ESP_OK) {
        const char *wday = (t.tm_wday >= 0 && t.tm_wday <= 6) ? wday_str[t.tm_wday] : "???";
        printf("Time: %04d-%02d-%02d %s %02d:%02d:%02d\n",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            wday,
            t.tm_hour, t.tm_min, t.tm_sec);
    }
}

/**
 * 设置 RX8130 的报警时间
 * @param hour    小时 [0-23]，填 -1 表示不设置
 * @param minute  分钟 [0-59]，填 -1 表示不设置
 * @param day     日期 [1-31]，填 -1 表示不设置
 * @param weekday 星期 [0-6]，0 表示星期日，填 -1 表示不设置
 * @return  返回 ESP_OK 表示成功，其他错误代码表示失败
 */
#define RX8130_ALARM_DISABLE_BIT ALARM_AE_BIT  // AE bit = 1 表示禁用比较此字段
esp_err_t rx8130_set_alarm(int weekday, int day, int hour, int minute)
{
    esp_err_t ret;
    uint8_t data[4];

    data[0] = (minute >= 0) ? dec2bcd(minute) : RX8130_ALARM_DISABLE_BIT;
    data[1] = (hour   >= 0) ? dec2bcd(hour)   : RX8130_ALARM_DISABLE_BIT;
    data[2] = (weekday>= 0) ? dec2bcd(weekday): RX8130_ALARM_DISABLE_BIT;
    data[3] = (day    >= 0) ? dec2bcd(day)    : RX8130_ALARM_DISABLE_BIT;

    ret = write_regs(REG_ALARM_MIN, data, 4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set alarm registers");
        return ret;
    }

    // Enable AF interrupt
    uint8_t tmp;
    ret = read_regs(REG_CONTROL0, &tmp, 1);
    if (ret != ESP_OK) return ret;
    tmp |= CTRL0_AIE; // AIE = 1
    ret = write_regs(REG_CONTROL0, &tmp, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable alarm interrupt");
        return ret;
    }

    return ESP_OK;
}

/**
 * 读取 RX8130 是否触发了报警
 * @param triggered 输出参数，true 表示已经触发报警
 * @return esp_err_t
 */
esp_err_t rx8130_get_alarm_flag(bool *flag)
{
    // 检查 AF 位是否被设置
    uint8_t tmp;
    esp_err_t ret = read_regs(REG_FLAG, &tmp, 1);
    *flag = ((tmp & FLAG_AF) != 0);

    return ret;
}

/**
 * 清除 RX8130 报警标志位
 * @return esp_err_t
 */
esp_err_t rx8130_clear_alarm_flag(void)
{
    // 清除 AF 位
    uint8_t tmp;
    esp_err_t ret = read_regs(REG_FLAG, &tmp, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read FLAG register");
        return ret;
    }
    tmp &= ~FLAG_AF; // 清除 AF 位
    ret = write_regs(REG_FLAG, &tmp, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear alarm flag");
    }

    return ret;
}

/**
 * 启动定时器
 * RX8130 内部包含一个定时器模块，可配置为 1Hz、64Hz、4096Hz、1/60Hz 的节拍频率。
 * 定时器在设定的 T 值减至 0 后可触发中断（TIMER_FLAG），适用于定时唤醒等功能。
 * 
 * 计时时长 = T 值 / 频率，例如：
 *  - frequency = 1Hz, timer_value = 5   → 时长 = 5秒
 *  - frequency = 64Hz, timer_value = 128 → 时长 = 2秒
 * 
 * @param timer_value  定时器初值（1~255），当减为 0 时触发中断
 * @param frequency    定时器频率（0=1/60Hz, 1=1Hz, 2=64Hz, 3=4096Hz）
 * @return esp_err_t
 *         - ESP_OK 成功
 *         - ESP_ERR_INVALID_ARG 参数无效
 *         - 其他 错误码
 */
esp_err_t rx8130_start_timer(uint8_t value, rx8130_timer_freq_t freq)
{
    esp_err_t ret;
    uint8_t tmp;

    if (value == 0 || freq > RX8130_TIMER_FREQ_1_60HZ) {
        return ESP_ERR_INVALID_ARG;
    }

    /* The datasheet requires TE=0 while loading the 16-bit preset. */
    ret = read_regs(REG_EXTENSION, &tmp, 1);
    if (ret != ESP_OK) return ret;
    tmp &= ~EXT_TE;
    ret = write_regs(REG_EXTENSION, &tmp, 1);
    if (ret != ESP_OK) return ret;

    /*
     * TBKE=0 keeps the wake timer counting in both VDD and VBAT modes.
     * This is required when the PMIC removes the main rail after shutdown.
     */
    ret = read_regs(REG_CONTROL0, &tmp, 1);
    if (ret != ESP_OK) return ret;
    tmp &= ~(CTRL0_TSTP | CTRL0_TBKON | CTRL0_TBKE);
    tmp |= CTRL0_TIE;
    ret = write_regs(REG_CONTROL0, &tmp, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable timer interrupt");
        return ret;
    }

    /* Counter1 is the high byte. Leaving an old value here can turn 3 s into
       several minutes, so always write and verify both bytes. */
    uint8_t counter[2] = {value, 0};
    ret = write_regs(REG_TIMER_COUNTER0, counter, sizeof(counter));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write timer counter preset");
        return ret;
    }
    uint8_t counter_verify[2] = {};
    ret = read_regs(REG_TIMER_COUNTER0, counter_verify, sizeof(counter_verify));
    if (ret != ESP_OK || counter_verify[0] != value || counter_verify[1] != 0) {
        ESP_LOGE(TAG, "Timer preset verify failed: expected=%u actual=0x%02X%02X",
                 value, counter_verify[1], counter_verify[0]);
        return ret == ESP_OK ? ESP_FAIL : ret;
    }
    
    // 读取当前EXTENSION寄存器，保留其他位
    ret = read_regs(REG_EXTENSION, &tmp, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read extension register");
        return ret;
    }
    
    ESP_LOGI(TAG, "当前EXTENSION寄存器: 0x%02X", tmp);
    
    // 清除FSEL、TE和TSEL位，然后设置新值
    tmp &= ~(EXT_FSEL1 | EXT_FSEL0 | EXT_TE | EXT_TSEL2 | EXT_TSEL1 | EXT_TSEL0);
    // FSEL=00(INT输出), TE=1(定时器使能), TSEL=freq
    tmp |= EXT_TE | (freq & 0x07);
    
    ret = write_regs(REG_EXTENSION, &tmp, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write extension register");
        return ret;
    }

    uint8_t control_verify = 0;
    uint8_t extension_verify = 0;
    if (read_regs(REG_CONTROL0, &control_verify, 1) != ESP_OK ||
        read_regs(REG_EXTENSION, &extension_verify, 1) != ESP_OK ||
        (control_verify & (CTRL0_TIE | CTRL0_TBKE)) != CTRL0_TIE ||
        (extension_verify & (EXT_TE | EXT_TSEL2 | EXT_TSEL1 | EXT_TSEL0)) !=
            (EXT_TE | (freq & 0x07))) {
        ESP_LOGE(TAG, "Timer config verify failed: CTRL0=0x%02X EXT=0x%02X",
                 control_verify, extension_verify);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "RX8130 timer armed: preset=%u freq=%u CTRL0=0x%02X EXT=0x%02X backup_count=enabled",
             value, freq, control_verify, extension_verify);
    return ESP_OK;
}

/**
 * 停止定时器
 */
esp_err_t rx8130_stop_timer(void)
{
    esp_err_t ret;
    uint8_t tmp;

    // Disable TF interrupt
    ret = read_regs(REG_CONTROL0, &tmp, 1);
    if (ret != ESP_OK) return ret;
    tmp &= ~CTRL0_TIE; // TIE = 0
    ret = write_regs(REG_CONTROL0, &tmp, 1);
    if (ret != ESP_OK) return ret;

    // Stop only the timer and preserve alarm/FOUT/update configuration.
    ret = read_regs(REG_EXTENSION, &tmp, 1);
    if (ret != ESP_OK) return ret;
    tmp &= ~EXT_TE;
    ret = write_regs(REG_EXTENSION, &tmp, 1);

    return ret; 
}

esp_err_t rx8130_get_timer_flag(bool *flag)
{
    uint8_t tmp;
    if (read_regs(REG_FLAG, &tmp, 1) != ESP_OK) {
        return ESP_FAIL;
    }
   *flag = ((tmp & FLAG_TF) != 0);

    return ESP_OK;
}

/**
 * 清除定时器中断标志 
 */
esp_err_t rx8130_clear_timer_flag(void)
{
    uint8_t flag_reg;
    if (read_regs(REG_FLAG, &flag_reg, 1) != ESP_OK) return ESP_FAIL;
    flag_reg &= ~FLAG_TF;  // 清除 TF 标志
    return write_regs(REG_FLAG, &flag_reg, 1);
}

// Extension register functions
esp_err_t rx8130_read_extension_register(uint8_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    return read_regs(REG_EXTENSION, data, 1);
}

esp_err_t rx8130_write_extension_register(uint8_t data)
{
    return write_regs(REG_EXTENSION, &data, 1);
}

esp_err_t rx8130_read_extension1_register(uint8_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    return read_regs(REG_EXTENSION1, data, 1);
}

esp_err_t rx8130_write_extension1_register(uint8_t data)
{
    return write_regs(REG_EXTENSION1, &data, 1);
}

// RAM register functions
esp_err_t rx8130_read_ram(uint8_t *data, size_t len)
{
    if (!data || len == 0 || len > 4) {
        return ESP_ERR_INVALID_ARG;
    }
    return read_regs(REG_RAM_START, data, len);
}

esp_err_t rx8130_write_ram(const uint8_t *data, size_t len)
{
    if (!data || len == 0 || len > 4) {
        return ESP_ERR_INVALID_ARG;
    }
    return write_regs(REG_RAM_START, (uint8_t*)data, len);
}

// Digital offset register functions
esp_err_t rx8130_read_digital_offset(uint8_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    return read_regs(REG_DIGITAL_OFFSET, data, 1);
}

esp_err_t rx8130_write_digital_offset(uint8_t data)
{
    return write_regs(REG_DIGITAL_OFFSET, &data, 1);
}

// Flag register functions
esp_err_t rx8130_read_flags(uint8_t *flags)
{
    if (!flags) {
        return ESP_ERR_INVALID_ARG;
    }
    return read_regs(REG_FLAG, flags, 1);
}

esp_err_t rx8130_clear_all_flags(void)
{
    uint8_t flags = 0x00;  // Clear all flags
    return write_regs(REG_FLAG, &flags, 1);
}

esp_err_t rx8130_clear_voltage_flags(void)
{
    uint8_t flags;
    esp_err_t ret = read_regs(REG_FLAG, &flags, 1);
    if (ret != ESP_OK) return ret;
    
    // Clear voltage-related flags
    flags &= ~(FLAG_VBLF | FLAG_VLF | FLAG_VBFF);
    return write_regs(REG_FLAG, &flags, 1);
}

// Control register functions
esp_err_t rx8130_read_control0(uint8_t *ctrl)
{
    if (!ctrl) {
        return ESP_ERR_INVALID_ARG;
    }
    return read_regs(REG_CONTROL0, ctrl, 1);
}

esp_err_t rx8130_write_control0(uint8_t ctrl)
{
    return write_regs(REG_CONTROL0, &ctrl, 1);
}

esp_err_t rx8130_read_control1(uint8_t *ctrl)
{
    if (!ctrl) {
        return ESP_ERR_INVALID_ARG;
    }
    return read_regs(REG_CONTROL1, ctrl, 1);
}

esp_err_t rx8130_write_control1(uint8_t ctrl)
{
    return write_regs(REG_CONTROL1, &ctrl, 1);
}

// Timer frequency selection
esp_err_t rx8130_set_timer_frequency(rx8130_timer_freq_t freq)
{
    uint8_t ext_reg;
    esp_err_t ret = read_regs(REG_EXTENSION, &ext_reg, 1);
    if (ret != ESP_OK) return ret;
    
    // Clear timer select bits and set new frequency
    ext_reg &= ~(EXT_TSEL2 | EXT_TSEL1 | EXT_TSEL0);
    ext_reg |= (freq & 0x07);
    
    return write_regs(REG_EXTENSION, &ext_reg, 1);
}

/**
 * 检查是否为闰年
 * RX8130自动处理闰年，但用户软件需要在2100,2200,2300年进行修正
 * @param year 年份（从1900年开始计算）
 * @return true 如果是闰年
 */
static bool is_leap_year(int year)
{
    int full_year = year + 1900;
    // 闰年规则：能被4整除，但不能被100整除，或者能被400整除
    return (full_year % 4 == 0 && full_year % 100 != 0) || (full_year % 400 == 0);
}

/**
 * 获取指定年份和月份的天数
 * @param year 年份（从1900年开始计算）
 * @param month 月份（0-11）
 * @return 该月的天数
 */
static int get_days_in_month(int year, int month)
{
    static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int days = days_in_month[month];
    
    // 2月需要检查闰年
    if (month == 1 && is_leap_year(year)) {
        days = 29;
    }
    
    return days;
}

static esp_err_t write_regs(uint8_t reg, uint8_t *data, size_t len)
{
    uint8_t write_buf[1 + len]; 
    write_buf[0] = reg;
    memcpy(&write_buf[1], data, len);

    return i2c_master_transmit(i2c_dev, write_buf, len + 1, 200); // 200ms timeout
}

static esp_err_t read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, 1, data, len, 200); // 200ms timeout
}

// 从 RTC 读出的 BCD 格式转十进制
static uint8_t bcd2dec(uint8_t bcd) 
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// 写入 RTC 前转为 BCD 格式
static uint8_t dec2bcd(uint8_t dec) 
{
    return ((dec / 10) << 4) | (dec % 10);
}

// 将 __DATE__ 和 __TIME__ 字符串解析为 struct tm
void get_compile_time_tm(struct tm *out)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    char month_str[4] = {0};
    int month = 0;
    int year = 0;

    // __DATE__ 格式: "MMM DD YYYY" (例如: "Jan 01 2025")
    sscanf(__DATE__, "%3s %d %d", month_str, &out->tm_mday, &year);
    for (int i = 0; i < 12; ++i) {
        if (strncmp(month_str, months[i], 3) == 0) {
            month = i;
            break;
        }
    }
    sscanf(__TIME__, "%d:%d:%d", &out->tm_hour, &out->tm_min, &out->tm_sec);

    out->tm_mon = month;
    out->tm_year = year - 1900;  // struct tm 需要的是从 1900 年开始的年数
    out->tm_isdst = -1;

    mktime(out);  // 自动计算 wday 等
}



