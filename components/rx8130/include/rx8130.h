
#pragma once   

#include <stdbool.h>
#include <time.h>
#include <esp_err.h>
#include "driver/i2c_master.h"

#ifdef __cplusplus   
extern "C" {
#endif

 

// RX8130 Register Definitions
#define REG_SEC          0x10 // 秒寄存器
#define REG_MIN          0x11 // 分钟寄存器   
#define REG_HOUR         0x12 // 小时寄存器
#define REG_WEEK         0x13 // 星期寄存器
#define REG_DAY          0x14 // 天寄存器
#define REG_MONTH        0x15 // 月份寄存器
#define REG_YEAR         0x16 // 年寄存器
#define REG_ALARM_MIN    0x17 // 报警分钟寄存器
#define REG_ALARM_HOUR   0x18 // 报警小时寄存器
#define REG_ALARM_WEEK   0x19 // 报警星期寄存器
#define REG_ALARM_DAY    0x1A // 报警天寄存器
#define REG_TIMER_COUNTER0 0x1A // 定时器计数器0
#define REG_TIMER_COUNTER1 0x1B // 定时器计数器1
#define REG_EXTENSION    0x1C // 扩展寄存器
#define REG_FLAG         0x1D // 标志寄存器
#define REG_CONTROL0     0x1E // 控制寄存器0
#define REG_CONTROL1     0x1F // 控制寄存器1
#define REG_RAM_START    0x20 // RAM起始地址
#define REG_RAM_END      0x23 // RAM结束地址
#define REG_DIGITAL_OFFSET 0x30 // 数字偏移寄存器
#define REG_EXTENSION1   0x31 // 扩展寄存器1

// Bit field definitions for registers

// SEC register (0x10) - bit 7 reserved, bits 6-0: seconds (0-59)
#define SEC_BIT_40       0x40
#define SEC_BIT_20       0x20
#define SEC_BIT_10       0x10
#define SEC_BIT_8        0x08
#define SEC_BIT_4        0x04
#define SEC_BIT_2        0x02
#define SEC_BIT_1        0x01

// MIN register (0x11) - bit 7 reserved, bits 6-0: minutes (0-59)
#define MIN_BIT_40       0x40
#define MIN_BIT_20       0x20
#define MIN_BIT_10       0x10
#define MIN_BIT_8        0x08
#define MIN_BIT_4        0x04
#define MIN_BIT_2        0x02
#define MIN_BIT_1        0x01

// HOUR register (0x12) - bits 7-6 reserved, bits 5-0: hours (0-23)
#define HOUR_BIT_20      0x20
#define HOUR_BIT_10      0x10
#define HOUR_BIT_8       0x08
#define HOUR_BIT_4       0x04
#define HOUR_BIT_2       0x02
#define HOUR_BIT_1       0x01

// WEEK register (0x13) - bit 7 reserved, bits 6-0: weekday (0-6)
#define WEEK_BIT_6       0x40
#define WEEK_BIT_5       0x20
#define WEEK_BIT_4       0x10
#define WEEK_BIT_3       0x08
#define WEEK_BIT_2       0x04
#define WEEK_BIT_1       0x02
#define WEEK_BIT_0       0x01

// DAY register (0x14) - bits 7-6 reserved, bits 5-0: day (1-31)
#define DAY_BIT_20       0x20
#define DAY_BIT_10       0x10
#define DAY_BIT_8        0x08
#define DAY_BIT_4        0x04
#define DAY_BIT_2        0x02
#define DAY_BIT_1        0x01

// MONTH register (0x15) - bits 7-5 reserved, bits 4-0: month (1-12)
#define MONTH_BIT_10     0x10
#define MONTH_BIT_8      0x08
#define MONTH_BIT_4      0x04
#define MONTH_BIT_2      0x02
#define MONTH_BIT_1      0x01

// YEAR register (0x16) - bits 7-0: year (0-99)
#define YEAR_BIT_80      0x80
#define YEAR_BIT_40      0x40
#define YEAR_BIT_20      0x20
#define YEAR_BIT_10      0x10
#define YEAR_BIT_8       0x08
#define YEAR_BIT_4       0x04
#define YEAR_BIT_2       0x02
#define YEAR_BIT_1       0x01

// Alarm registers - AE bit enables alarm comparison
#define ALARM_AE_BIT     0x80  // Alarm Enable bit

// Timer Counter 0 (0x1A) - 8-bit counter
#define TIMER0_BIT_128   0x80
#define TIMER0_BIT_64    0x40
#define TIMER0_BIT_32    0x20
#define TIMER0_BIT_16    0x10
#define TIMER0_BIT_8     0x08
#define TIMER0_BIT_4     0x04
#define TIMER0_BIT_2     0x02
#define TIMER0_BIT_1     0x01

// Timer Counter 1 (0x1B) - 16-bit counter (high byte)
#define TIMER1_BIT_32768 0x80
#define TIMER1_BIT_16384 0x40
#define TIMER1_BIT_8192  0x20
#define TIMER1_BIT_4096  0x10
#define TIMER1_BIT_2048  0x08
#define TIMER1_BIT_1024  0x04
#define TIMER1_BIT_512   0x02
#define TIMER1_BIT_256   0x01

// Extension Register (0x1C)
#define EXT_FSEL1        0x80  // Frequency select 1
#define EXT_FSEL0        0x40  // Frequency select 0
#define EXT_USEL         0x20  // User select
#define EXT_TE           0x10  // Timer enable
#define EXT_WADA         0x08  // Week alarm day alarm
#define EXT_TSEL2        0x04  // Timer select 2
#define EXT_TSEL1        0x02  // Timer select 1
#define EXT_TSEL0        0x01  // Timer select 0

// Flag Register (0x1D)
#define FLAG_VBLF        0x80  // Voltage backup low flag
#define FLAG_UF          0x20  // Update flag
#define FLAG_TF           0x10  // Timer flag
#define FLAG_AF           0x08  // Alarm flag
#define FLAG_RSF          0x04  // Reset flag
#define FLAG_VLF          0x02  // Voltage low flag
#define FLAG_VBFF         0x01  // Voltage backup flag

// Control Register 0 (0x1E)
#define CTRL0_TEST       0x80  // Test mode
#define CTRL0_STOP       0x40  // Stop
#define CTRL0_UIE        0x20  // Update interrupt enable
#define CTRL0_TIE        0x10  // Timer interrupt enable
#define CTRL0_AIE        0x08  // Alarm interrupt enable
#define CTRL0_TSTP       0x04  // Timer stop
#define CTRL0_TBKON      0x02  // Timer backup on
#define CTRL0_TBKE       0x01  // Timer backup enable

// Control Register 1 (0x1F)
#define CTRL1_SMP_TSEL1  0x80  // Sample timer select 1
#define CTRL1_SMP_TSEL0  0x40  // Sample timer select 0
#define CTRL1_CHG_EN     0x20  // Charge enable
#define CTRL1_INIEN      0x10  // Initial enable
#define CTRL1_RS          // Reserved
#define CTRL1_VSEL_BF    0x04  // Voltage select backup
#define CTRL1_VSEL1_BF   0x02  // Voltage select 1 backup
#define CTRL1_VSEL0_BF   0x01  // Voltage select 0 backup

// Digital Offset Register (0x30)
#define DOFF_DTE         0x80  // Digital temperature enable
#define DOFF_L7          0x40  // L7
#define DOFF_L6          0x20  // L6
#define DOFF_L5          0x10  // L5
#define DOFF_L4          0x08  // L4
#define DOFF_L3          0x04  // L3
#define DOFF_L2          0x02  // L2
#define DOFF_L1          0x01  // L1

// Extension Register 1 (0x31)
#define EXT1_VBLFE       0x01  // Voltage backup low flag enable

// RX8130 Year Register Notes:
// - YEAR register stores BCD counter for years 00 to 99
// - Century bit (bit 7 of MONTH register) indicates:
//   - 0: 1900-1999 years
//   - 1: 2000-2099 years
// - Leap years are automatically handled by RX8130 for 00,04,08,12...96
// - User software correction needed for years 2100,2200,2300 (common years) 

typedef enum {
    RX8130_TIMER_FREQ_4096HZ = 0,
    RX8130_TIMER_FREQ_64HZ   = 1,
    RX8130_TIMER_FREQ_1HZ    = 2,
    RX8130_TIMER_FREQ_1_60HZ = 3
} rx8130_timer_freq_t;

typedef void (*rx8130_irq_cb_t)(void);

esp_err_t rx8130_init(i2c_master_bus_handle_t i2c_bus, gpio_num_t int_pin);
esp_err_t rx8130_deinit(void);

esp_err_t rx8130_set_time(const struct tm *time);
esp_err_t rx8130_get_time(struct tm *time);

esp_err_t rx8130_set_irq_callback(rx8130_irq_cb_t cb);

esp_err_t rx8130_set_alarm(int weekday, int day, int hour, int minute);
esp_err_t rx8130_get_alarm_flag(bool *triggered);
esp_err_t rx8130_clear_alarm_flag(void);

esp_err_t rx8130_start_timer(uint8_t value, rx8130_timer_freq_t freq);
esp_err_t rx8130_stop_timer(void);
esp_err_t rx8130_get_timer_flag(bool *flag);
esp_err_t rx8130_clear_timer_flag(void);

void rx8130_print_time(void);
void get_compile_time_tm(struct tm *out);

// Extension register functions
esp_err_t rx8130_read_extension_register(uint8_t *data);
esp_err_t rx8130_write_extension_register(uint8_t data);
esp_err_t rx8130_read_extension1_register(uint8_t *data);
esp_err_t rx8130_write_extension1_register(uint8_t data);

// RAM register functions
esp_err_t rx8130_read_ram(uint8_t *data, size_t len);
esp_err_t rx8130_write_ram(const uint8_t *data, size_t len);

// Digital offset register functions
esp_err_t rx8130_read_digital_offset(uint8_t *data);
esp_err_t rx8130_write_digital_offset(uint8_t data);

// Flag register functions
esp_err_t rx8130_read_flags(uint8_t *flags);
esp_err_t rx8130_clear_all_flags(void);
esp_err_t rx8130_clear_voltage_flags(void);

// Control register functions
esp_err_t rx8130_read_control0(uint8_t *ctrl);
esp_err_t rx8130_write_control0(uint8_t ctrl);
esp_err_t rx8130_read_control1(uint8_t *ctrl);
esp_err_t rx8130_write_control1(uint8_t ctrl);

// Timer frequency selection
esp_err_t rx8130_set_timer_frequency(rx8130_timer_freq_t freq);



#ifdef __cplusplus
}
#endif



