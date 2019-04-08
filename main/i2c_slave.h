#pragma once
#include <driver/gpio.h>

#define I2C_SLAVE_SDA_IO     GPIO_NUM_14
#define I2C_SLAVE_SCL_IO     GPIO_NUM_33
#define I2C_SLAVE_SDA_PULLUP GPIO_PULLUP_ENABLE
#define I2C_SLAVE_SCL_PULLUP GPIO_PULLUP_ENABLE
#define I2C_SLAVE_NUM        I2C_NUM_1
#define I2C_SLAVE_RX_BUF_LEN 512
#define I2C_SLAVE_ADDRESS    0x40

typedef void (*i2c_slave_callback_t)(const uint8_t* data, size_t len);

void i2c_slave_init(i2c_slave_callback_t cb);
