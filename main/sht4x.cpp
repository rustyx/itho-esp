#include "sht4x.h"
#include <stdint.h>
#include <sys/types.h>
#include <esp_log.h>

static char TAG[] = "SHT4x";

#define SHT4X_CMD_MEASURE_HPM 0xFD
#define SHT4X_CMD_MEASURE_LPM 0xE0
#define SHT4X_CMD_READ_SERIAL 0x89
#define SHT4X_CMD_DURATION_USEC 1000
#define SHT4X_MEASUREMENT_DURATION_HPM_USEC 9000
#define SHT4X_MEASUREMENT_DURATION_LPM_USEC 2000
#define SHT4X_ADDRESS 0x44

SHT4x::SHT4x(gpio_num_t i2c_sda, gpio_num_t i2c_scl) : BitbangI2C(i2c_sda, i2c_scl) {
    ESP_LOGI(TAG, "Starting SHT4x on GPIO: SDA %d, SCL %d", i2c_sda, i2c_scl);
}

static uint8_t crc8(const uint8_t* data, int len) {
    uint8_t crc = 0xFF;
    for (int j = len; j; --j) {
        crc ^= *data++;
        for (int i = 8; i; --i) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}

bool SHT4x::errorHandler(i2c_err_t response) {
    switch (response) {
    case i2c_err_t::OK:
        return true;
    case i2c_err_t::START_TIMEOUT:
        ESP_LOGE(TAG, "i2c bus error");
        break;
    case i2c_err_t::DEVICE_NACK:
        ESP_LOGE(TAG, "device timeout");
        break;
    case i2c_err_t::WRITE_NACK:
        ESP_LOGE(TAG, "command rejected");
        break;
    case i2c_err_t::READ_NACK:
        ESP_LOGE(TAG, "read error");
        break;
    case i2c_err_t::CRC_ERROR:
        ESP_LOGE(TAG, "CRC error");
        break;
    }

    return false;
}

i2c_err_t SHT4x::measure() {
    uint8_t buf[6];

    portENTER_CRITICAL(&mutex);
    i2c_err_t res = i2c_write(SHT4X_ADDRESS, SHT4X_CMD_MEASURE_HPM);
    if (res != i2c_err_t::OK) {
        portEXIT_CRITICAL(&mutex);
        return res;
    }
    ets_delay_us(SHT4X_MEASUREMENT_DURATION_HPM_USEC);

    res = i2c_read(SHT4X_ADDRESS, buf, sizeof(buf));
    if (res == i2c_err_t::OK) {
        if (crc8(buf, 2) != buf[2] || crc8(buf + 3, 2) != buf[5]) {
            res = i2c_err_t::CRC_ERROR;
        }
        /**
         * temperature = 175 * S_T / 65535 - 45
         * humidity = 125 * (S_RH / 65535) - 6
         */
        // xx.x fixed point
        temperature = ((875 * ((buf[0] << 8) | buf[1])) >> 15) - 450;
        humidity = ((625 * ((buf[3] << 8) | buf[4])) >> 15) - 60;
        // xx.xxx fixed point
        // temperature = ((21875 * ((buf[0] << 8) | buf[1])) >> 13) - 45000;
        // humidity = ((15625 * ((buf[3] << 8) | buf[4])) >> 13) - 6000;
    }
    portEXIT_CRITICAL(&mutex);

    return res;
}

i2c_err_t SHT4x::readSerial() {
    uint8_t buf[6];

    portENTER_CRITICAL(&mutex);
    i2c_err_t res = i2c_write(SHT4X_ADDRESS, SHT4X_CMD_READ_SERIAL);
    if (res != i2c_err_t::OK) {
        portEXIT_CRITICAL(&mutex);
        return res;
    }
    ets_delay_us(SHT4X_CMD_DURATION_USEC);

    res = i2c_read(SHT4X_ADDRESS, buf, sizeof(buf));
    if (res == i2c_err_t::OK) {
        if (crc8(buf, 2) != buf[2] || crc8(buf + 3, 2) != buf[5]) {
            res = i2c_err_t::CRC_ERROR;
        }
        serial = (buf[0] << 24u) | (buf[1] << 16u) | (buf[3] << 8u) | buf[4];
    }
    portEXIT_CRITICAL(&mutex);

    return res;
}
