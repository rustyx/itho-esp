#include "i2c_slave.h"
#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

static const char* TAG = "i2c-slave";

static i2c_slave_callback_t i2c_callback;
static uint8_t buf[I2C_SLAVE_RX_BUF_LEN];
static size_t buflen;

static void i2c_slave_task(void* arg) {
    while (1) {
        buf[0] = I2C_SLAVE_ADDRESS << 1;
        buflen = 1;
        while (1) {
            int len1 = i2c_slave_read_buffer(I2C_SLAVE_NUM, buf + buflen, sizeof(buf) - buflen, 10);
            if (len1 <= 0)
                break;
            buflen += len1;
        }
        if (buflen > 1) {
            i2c_callback(buf, buflen);
        }
    }
}

void i2c_slave_init(i2c_slave_callback_t cb) {
    i2c_config_t conf = {I2C_MODE_SLAVE,   I2C_SLAVE_SDA_IO,     I2C_SLAVE_SDA_PULLUP,
                         I2C_SLAVE_SCL_IO, I2C_SLAVE_SCL_PULLUP, {.slave = {0, I2C_SLAVE_ADDRESS}}};
    i2c_param_config(I2C_SLAVE_NUM, &conf);
    i2c_callback = cb;
    i2c_driver_install(I2C_SLAVE_NUM, conf.mode, I2C_SLAVE_RX_BUF_LEN, 0, 0);
    ESP_LOGI(TAG, "Slave pin assignment: SCL=%d, SDA=%d", I2C_SLAVE_SCL_IO, I2C_SLAVE_SDA_IO);
    ESP_LOGI(TAG, "Slave address: %02X (W)", I2C_SLAVE_ADDRESS << 1);
    xTaskCreatePinnedToCore(i2c_slave_task, "i2c_task", 4096, NULL, 22, NULL, 0);
}
