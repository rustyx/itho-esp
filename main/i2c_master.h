#pragma once
#include <driver/gpio.h>

#define I2C_MASTER_SDA_IO     GPIO_NUM_27
#define I2C_MASTER_SCL_IO     GPIO_NUM_26
#define I2C_MASTER_SDA_PULLUP GPIO_PULLUP_ENABLE
#define I2C_MASTER_SCL_PULLUP GPIO_PULLUP_ENABLE
#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_FREQ_HZ    100000

void i2c_master_init();
esp_err_t i2c_master_send(char* buf, uint32_t len);
