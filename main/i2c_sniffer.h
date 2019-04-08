#pragma once
#include <driver/gpio.h>

#define I2C_SNIFFER_SDA_PIN      13
#define I2C_SNIFFER_SCL_PIN      25
#define I2C_SNIFFER_RUN_ON_CORE  1
#define I2C_SNIFFER_PRINT_TIMING 0

void i2c_sniffer_init(bool enabled);
void i2c_sniffer_enable();
void i2c_sniffer_disable();
void i2c_sniffer_pullup(bool enable);
