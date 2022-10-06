#include "i2c_master.h"
#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

static const char* TAG = "i2c-master";

void i2c_master_init() {
    i2c_config_t conf {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = I2C_MASTER_SDA_PULLUP;
    conf.scl_pullup_en = I2C_MASTER_SCL_PULLUP;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    ESP_LOGI(TAG, "Master pin assignment: SCL=%d, SDA=%d", I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
}

esp_err_t i2c_master_send(char* buf, uint32_t len) {
    esp_err_t rc;
    for (uint32_t i = 0; i < 5; ++i) {
        i2c_cmd_handle_t link = i2c_cmd_link_create();
        i2c_master_start(link);
        i2c_master_write(link, (uint8_t*)buf, len, true);
        i2c_master_stop(link);
        rc = i2c_master_cmd_begin(I2C_MASTER_NUM, link, 25);
        i2c_cmd_link_delete(link);
        if (!rc)
            break;
        vTaskDelay(1);
    }
    return rc;
}
