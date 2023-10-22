#include "i2c.h"
#include <esp_err.h>
#include <esp_log.h>
#include <stdint.h>
#include <sys/types.h>
#include <freertos/task.h>

#define I2C_CLOCK_HALF_US 10
#define I2C_CLOCK_IDLE_US (I2C_CLOCK_HALF_US * 2)
#define I2C_CLOCK_HOLD_US (I2C_CLOCK_HALF_US * 2)
#define I2C_CLOCK_WAIT_MAX_US 100000

BitbangI2C::BitbangI2C(gpio_num_t i2c_sda, gpio_num_t i2c_scl)
      : i2c_sda(i2c_sda),
        i2c_scl(i2c_scl) {
    gpio_config_t cfg {
        .pin_bit_mask = BIT64(i2c_sda) | BIT64(i2c_scl),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(i2c_sda, 1);
    gpio_set_level(i2c_scl, 1);
    vTaskDelay(10);
}

i2c_err_t IRAM_ATTR BitbangI2C::i2c_write(uint8_t addr, uint8_t data) {
    i2c_err_t res = i2c_start();
    if (res != i2c_err_t::OK) {
        return res;
    }
    res = i2c_write_byte(addr << 1);
    if (res != i2c_err_t::OK) {
        i2c_stop();
        return res == i2c_err_t::WRITE_NACK ? i2c_err_t::DEVICE_NACK : res;
    }
    res = i2c_write_byte(data);
    i2c_stop();

    return res;
}

i2c_err_t IRAM_ATTR BitbangI2C::i2c_start() {
    // SDA 1
    gpio_set_level(i2c_sda, 1);
    // SCL 1 (wait)
    gpio_set_level(i2c_scl, 1);
    int idle_us = 0;
    for (int i = I2C_CLOCK_WAIT_MAX_US; i; --i) {
        if (gpio_get_level(i2c_scl)) {
            if (++idle_us >= I2C_CLOCK_IDLE_US) {
                break;
            }
        } else {
            idle_us = 0;
        }
        if (i == 1) {
            return i2c_err_t::START_TIMEOUT;
        }
        ets_delay_us(1);
    }
    // SDA 0
    gpio_set_level(i2c_sda, 0);
    ets_delay_us(I2C_CLOCK_HOLD_US);

    return i2c_err_t::OK;
}

void IRAM_ATTR BitbangI2C::i2c_stop() {
    // SCL 0
    gpio_set_level(i2c_scl, 0);
    ets_delay_us(1);
    // SDA 0
    gpio_set_level(i2c_sda, 0);
    ets_delay_us(I2C_CLOCK_HALF_US - 1);
    // SCL 1
    gpio_set_level(i2c_scl, 1);
    ets_delay_us(I2C_CLOCK_HALF_US);
    // SDA 1
    gpio_set_level(i2c_sda, 1);
    ets_delay_us(I2C_CLOCK_HALF_US);
}

i2c_err_t IRAM_ATTR BitbangI2C::i2c_write_byte(uint8_t data) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
        // SCL 0
        gpio_set_level(i2c_scl, 0);
        ets_delay_us(1);
        // SDA x
        gpio_set_level(i2c_sda, !!(data & mask));
        ets_delay_us(I2C_CLOCK_HALF_US - 1);
        // SCL 1
        gpio_set_level(i2c_scl, 1);
        ets_delay_us(I2C_CLOCK_HALF_US);
    }
    gpio_set_level(i2c_scl, 0);
    ets_delay_us(1);
    gpio_set_level(i2c_sda, 1);
    ets_delay_us(I2C_CLOCK_HALF_US - 1);
    gpio_set_level(i2c_scl, 1);
    ets_delay_us(I2C_CLOCK_HALF_US - 1);
    int res = gpio_get_level(i2c_sda);
    ets_delay_us(1);

    return res ? i2c_err_t::WRITE_NACK : i2c_err_t::OK;
}

std::pair<i2c_err_t, uint8_t> IRAM_ATTR BitbangI2C::i2c_read_byte(bool ack) {
    std::pair<i2c_err_t, uint8_t> res {};
    for (int i = 0; i < 8; ++i) {
        // SCL 0
        gpio_set_level(i2c_scl, 0);
        ets_delay_us(1);
        // SDA 1
        gpio_set_level(i2c_sda, 1);
        ets_delay_us(I2C_CLOCK_HALF_US - 1);
        // SCL 1
        gpio_set_level(i2c_scl, 1);
        ets_delay_us(I2C_CLOCK_HALF_US / 2);
        // read SDA
        res.second = (res.second << 1) | gpio_get_level(i2c_sda);
        ets_delay_us(I2C_CLOCK_HALF_US - I2C_CLOCK_HALF_US / 2);
        // ets_delay_us(1);
    }
    // SCL 0
    gpio_set_level(i2c_scl, 0);
    ets_delay_us(1);
    // SDA 1
    gpio_set_level(i2c_sda, !ack);
    ets_delay_us(I2C_CLOCK_HALF_US - 1);
    // SCL 1
    gpio_set_level(i2c_scl, 1);
    ets_delay_us(I2C_CLOCK_HALF_US / 2);
    // read SDA
    if (gpio_get_level(i2c_sda)) {
        res.first = i2c_err_t::READ_NACK;
    }
    ets_delay_us(I2C_CLOCK_HALF_US - I2C_CLOCK_HALF_US / 2);

    return res;
}

i2c_err_t IRAM_ATTR BitbangI2C::i2c_read(uint8_t addr, uint8_t* buf, int len) {
    i2c_err_t res = i2c_start();
    if (res != i2c_err_t::OK) {
        return res;
    }
    res = i2c_write_byte((addr << 1) | 1);
    if (res != i2c_err_t::OK) {
        i2c_stop();
        return res == i2c_err_t::WRITE_NACK ? i2c_err_t::DEVICE_NACK : res;
    }
    uint8_t* p = buf;
    for (int i = len; i; --i) {
        auto b = i2c_read_byte(i > 1);
        *p++ = b.second;
    }
    i2c_stop();

    return res;
}
