#pragma once
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <utility>

enum class i2c_err_t {
    OK = 0,
    START_TIMEOUT = -1,
    DEVICE_NACK = -2,
    WRITE_NACK = -3,
    READ_NACK = -4,
    CRC_ERROR = -5,
};

class BitbangI2C {
public:
    BitbangI2C(gpio_num_t i2c_sda, gpio_num_t i2c_scl);
    i2c_err_t i2c_write(uint8_t addr, uint8_t data);
    i2c_err_t i2c_start();
    void i2c_stop();
    i2c_err_t i2c_write_byte(uint8_t data);
    std::pair<i2c_err_t, uint8_t> i2c_read_byte(bool ack);
    i2c_err_t i2c_read(uint8_t addr, uint8_t* buf, int len);
protected:
    portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
    gpio_num_t i2c_sda;
    gpio_num_t i2c_scl;
};
