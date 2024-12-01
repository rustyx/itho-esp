#pragma once
#include "i2c.h"

// Sensirion SHT4x (SHT40, SHT41, SHT43, SHT45)

class SHT4x : BitbangI2C {
public:
    SHT4x(const char* tag, gpio_num_t i2c_sda, gpio_num_t i2c_scl);
    bool errorHandler(i2c_err_t response);
    i2c_err_t measure();
    int getHumidity() const { return humidity; }
    int getTemperature() const { return temperature; }
    i2c_err_t readSerial();
    uint32_t getSerial() const { return serial; }

private:
    char tag[12];
    int humidity = 0;
    int temperature = 0;
    uint32_t serial = 0;
};
