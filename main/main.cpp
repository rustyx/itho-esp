#include "Config.h"
#include "Nvs.h"
#include "console.h"
#include "dht.h"
#include "sht4x.h"
#include "i2c_master.h"
#include "i2c_slave.h"
#include "i2c_sniffer.h"
#include "mqtt.h"
#include "util.h"
#include "wifi.h"
#include <driver/uart.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <jsmn.h>

static const char* TAG = "main";

static char cmd[128];
static uint8_t buf[128];
static uint32_t cmdlen = 0;
static int dht_ret = -1, hum = -1, temp = -1;
static bool verbose = false, reportHex = false;

Nvs nvs;
Config config;

bool sendBytes(const uint8_t* buf, size_t len) {
    if (len) {
        esp_err_t rc = i2c_master_send((char*)buf, len);
        if (rc || verbose) {
            printf("Master send: %d\n", rc);
        }
        return rc == ESP_OK;
    }
    return false;
}

bool sendBytesHex(const char* hex, size_t hexlen) {
    size_t len = parseHexStr(hex, hexlen, buf, sizeof(buf));
    return sendBytes(buf, len);
}

static void publishHumidity() {
    char buf[32];
    snprintf(buf, sizeof(buf), "[%d.%d,%d.%d,%d]", hum / 10, hum % 10, temp / 10, temp % 10, (int)dht_ret);
    mqtt_publish("esp-data-dht", buf);
}

static uint8_t set1[]{0x82, 0x60, 0xC1, 0x01, 0x01, 0x11, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
                      0xFF, 0xFF, 0x00, 0x22, 0xF1, 0x03, 0x00, 0x02, 0x04, 0x00, 0x00, 0xCC};
static uint8_t set2[]{0x82, 0x60, 0xC1, 0x01, 0x01, 0x11, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
                      0xFF, 0xFF, 0x00, 0x22, 0xF1, 0x03, 0x00, 0x03, 0x04, 0x00, 0x00, 0xCC};
static uint8_t set3[]{0x82, 0x60, 0xC1, 0x01, 0x01, 0x11, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
                      0xFF, 0xFF, 0x00, 0x22, 0xF3, 0x03, 0x00, 0x00, 0x1E, 0x00, 0x00, 0xCC};

static void initRFTKey(uint32_t rftKey) {
    memcpy(set1 + 10, &rftKey, 4);
    fixChecksum(set1, sizeof(set1));
    memcpy(set2 + 10, &rftKey, 4);
    fixChecksum(set2, sizeof(set2));
    memcpy(set3 + 10, &rftKey, 4);
    fixChecksum(set3, sizeof(set3));
}

static int64_t lastSetTime;

static void processCommand(const char* data, int data_len) {
    if (strncmp("set1", data, data_len) == 0) {
        lastSetTime = std::max(lastSetTime, esp_timer_get_time()) + 3600 * 1000000LL;
        sendBytes(set1, sizeof(set1));
    } else if (strncmp("set2", data, data_len) == 0) {
        sendBytes(set2, sizeof(set2));
    } else if (strncmp("set3", data, data_len) == 0) { // 30min
        sendBytes(set3, sizeof(set3));
    }
    // } else if (strncmp("sniff0", data, data_len) == 0) {
    //     i2c_sniffer_disable();
    //     reportHex = false;
    //     mqtt_publish("esp-data", "sniffer off");
    // } else if (strncmp("sniff1", data, data_len) == 0) {
    //     i2c_sniffer_enable();
    //     reportHex = true;
    //     mqtt_publish("esp-data", "sniffer on");
    // } else if (strncmp("rftkey", data, 6) == 0) {
    //     unsigned i = 6;
    //     while (data[i] == ' ')
    //         i++;
    //     if (i < data_len) {
    //         uint32_t key = 0;
    //         parseHexStr(data + i, data_len - i, (uint8_t*)&key, 4);
    //         if (key) {
    //             config.rftKey = key;
    //             initRFTKey(config.rftKey);
    //             if (!config.Write()) {
    //                 ESP_LOGE(TAG, "Config write failed");
    //             }
    //         }
    //     }
    //     std::string s = "rftkey=";
    //     s += toHexStr((uint8_t*)&config.rftKey, 4);
    //     mqtt_publish("esp-data", s.c_str());
    else if (strncmp("high_hum_threshold", data, 18) == 0) {
        unsigned i = 18;
        while (data[i] == ' ')
            i++;
        if (i < data_len) {
            std::string hum_str(data + i, data + data_len);
            int hum = atoi(hum_str.c_str());
            if (hum < 100 || hum > 1000) {
                std::string buf = "invalid parameter value " + hum_str;
                ESP_LOGE(TAG, "%s", buf.c_str());
                mqtt_publish("esp-data-dht", buf.c_str());
                return;
            }
            config.high_hum_threshold = Config::normalize_high_hum_threshold(hum);
            if (!config.Write()) {
                ESP_LOGE(TAG, "Config write failed");
            }
        }
        int hum1 = config.high_hum_threshold / 10, hum2 = config.high_hum_threshold % 10;
        std::string buf = "high_hum_threshold=" + std::to_string(hum1) + '.' + std::to_string(hum2);
        ESP_LOGI(TAG, "%s", buf.c_str());
        mqtt_publish("esp-data-dht", buf.c_str());
    } else if (strncmp("hum", data, data_len) == 0) {
        publishHumidity();
    } else if (isHex(data[0])) {
        sendBytesHex(data, data_len);
    } else {
        ESP_LOGE(TAG, "Unknown command '%s'", std::string(data, data + data_len).c_str());
    }
}

static const int mode_set_max_freq_sec = 600; // set mode at most once every 10 min
static int prev_hum;

static void handleHumidity() {
    if (dht_ret != DHT_OK) {
        return;
    }
    if (hum >= config.high_hum_threshold && prev_hum >= config.high_hum_threshold) {
        int64_t now = esp_timer_get_time();
        if (now - lastSetTime >= mode_set_max_freq_sec * 1000000) {
            ESP_LOGI(TAG, "High humidity, setting mode 3");
            processCommand("set3", 4);
        }
    }
    prev_hum = hum;
}

static void processConsoleCommand() {
    if (strcmp(cmd, "p") == 0) {
        i2c_sniffer_pullup(false);
        printf("Pullup OFF\n");
    } else if (strcmp(cmd, "P") == 0) {
        i2c_sniffer_pullup(true);
        printf("Pullup ON\n");
    } else if (strcmp(cmd, "s") == 0) {
        i2c_sniffer_disable();
        printf("Sniffer OFF\n");
    } else if (strcmp(cmd, "S") == 0) {
        i2c_sniffer_enable();
        printf("Sniffer ON\n");
    } else if (strcmp(cmd, "h") == 0) {
        reportHex = false;
        printf("esp-data-hex reporting OFF\n");
    } else if (strcmp(cmd, "H") == 0) {
        reportHex = true;
        printf("esp-data-hex reporting ON\n");
    } else if (strcmp(cmd, "r") == 0) {
        printf("Restarting\n");
        vTaskDelay(configTICK_RATE_HZ / 4);
        esp_restart();
    } else if (strcmp("config", cmd) == 0) {
        if (config.Reconfigure()) {
            i2c_sniffer_enable();
            printf("Sniffer is ON.\n"
                   "Press any key on the RFT device, watching for a message that looks like:\n"
                   "82 60 C1 01 01 11 .. .. .. .. xx xx xx xx .. .. .. .. .. .. .. .. .. ..\n"
                   "The RFT ID will be the values xx xx xx xx.\n"
                   "Write those down, then turn off the sniffer with the 's' command.\n");
        }
    } else {
        processCommand(cmd, strlen(cmd));
    }
}

inline bool eq(const char* json, const jsmntok_t& tok, const char* s) {
    return (tok.type == JSMN_STRING && strncmp(json + tok.start, s, tok.end - tok.start) == 0);
}

portMUX_TYPE status_mux = portMUX_INITIALIZER_UNLOCKED;
std::string datatypes;

static void requestStatus() {
    for (int i = 0; i < 3; ++i) {
        portENTER_CRITICAL(&status_mux);
        bool haveDatatypes = !datatypes.empty();
        portEXIT_CRITICAL(&status_mux);
        if (haveDatatypes)
            break;
        sendBytesHex("82 80 A4 00 04 00 56", 20);
        vTaskDelay(configTICK_RATE_HZ / 5);
    }
    if (datatypes.empty()) {
        ESP_LOGW(TAG, "Unable to fetch data types");
    }
    sendBytesHex("82 80 A4 01 04 00 55", 20);
}

static void requestStatusTask(void* arg) {
    requestStatus();
    vTaskDelete(nullptr);
}

static void requestStatusLoopTask(void* arg) {
    for (;;) {
        vTaskDelay(5 * configTICK_RATE_HZ);
        if (mqtt_is_connected()) {
            requestStatus();
        }
    }
    vTaskDelete(nullptr);
}

/*
    Data format (1 byte):
    bit 7: signed / unsigned
    bits 6..4: size (2^n): 0=1, 1=2, 2=4
    bits 3..0: decimal digits (divider 10^n)
*/
void handleStatus(const uint8_t* data, size_t len) {
    std::string s;
    char buf[64];
    s.reserve(4 * len + 4);
    s = "[";
    size_t i = 0;
    for (uint8_t dt : datatypes) {
        if (i >= len)
            break;
        if (s.length() > 1)
            s += ',';
        int32_t x = (dt & 0x80) ? (int8_t)data[i++] : data[i++];
        for (int j = (1 << ((dt >> 4) & 3)); j > 1; --j) {
            x = (x << 8) | data[i++];
        }
        int rem = dt & 7;
        snprintf(buf, sizeof(buf), "%0*d", rem + 1 + (x < 0), x);
        size_t len = strlen(buf);
        s.append(buf, len - rem);
        if (rem) {
            s += '.';
            s.append(buf + len - rem, rem);
        }
    }
    snprintf(buf, sizeof(buf), ",%d.%d,%d.%d,%d]", hum / 10, hum % 10, temp / 10, temp % 10, (int)dht_ret);
    s += buf;
    mqtt_publish_bin("esp-data", s.c_str(), s.length());
}

inline bool checksumOk(const uint8_t* data, size_t len) { return checksum(data, len - 1) == data[len - 1]; }

void i2c_slave_callback(const uint8_t* data, size_t len) {
    std::string s = toHexStr(data, len);
    if (len > 6 && data[1] == 0x82 && data[2] == 0xA4 && data[3] == 0 && data[4] == 1 && data[5] < len - 5 && checksumOk(data, len)) {
        portENTER_CRITICAL(&status_mux);
        datatypes.assign((const char*)data + 6, data[5]);
        portEXIT_CRITICAL(&status_mux);
    }
    if (len > 6 && data[1] == 0x82 && data[2] == 0xa4 && data[3] == 1 && data[4] == 1 && data[5] < len - 5 && checksumOk(data, len)) {
        handleStatus(data + 6, len - 7);
    }
    if (reportHex) {
        mqtt_publish_bin("esp-data-hex", s.c_str(), s.length());
    }
    if (verbose) {
        printf("Slave: %s\n", s.c_str());
    }
}

static void processMqttCommand(const char* data, int data_len) {
    if (strncmp("status", data, data_len) == 0) {
        xTaskCreate(requestStatusTask, "requestStatusTask", 4096, NULL, 6, NULL);
    } else if (strncmp("ping", data, data_len) == 0) {
        mqtt_publish("esp-data", "pong");
    } else {
        processCommand(data, data_len);
    }
}

static void mqtt_message_callback(const char* topic, int topic_len, const char* data, int data_len) {
    processMqttCommand(data, data_len);
    // ESP_LOGD(TAG, "mqtt_callback '%.*s' '%.*s'", topic_len, topic, data_len, data);
}

static void mqtt_connect_callback() { mqtt_subscribe("esp", mqtt_message_callback); }

static void dht_task(void* arg) {
    DHT dht((gpio_num_t)config.dht_gpio);
    int errors = 1;
    while (1) {
        vTaskDelay(5 * configTICK_RATE_HZ); // wait at least 2 sec before reading again
        dht_ret = dht.readDHT();
        if (!dht.errorHandler(dht_ret)) {
            if (errors && ++errors > 3) {
                ESP_LOGI(TAG, "Stopping DHT task");
                break;
            }
            continue;
        }
        errors = 0;
        hum = dht.getHumidity(), temp = dht.getTemperature();
        ESP_LOGI(TAG, "Humidity %d.%d, Temp %d.%d", hum / 10, hum % 10, temp / 10, temp % 10);
        handleHumidity();
    }
    vTaskDelete(nullptr);
}

#define SHT4x_GPIO_SDA GPIO_NUM_21
#define SHT4x_GPIO_SCL GPIO_NUM_22

static void sht4x_task(void* arg) {
    SHT4x sht((gpio_num_t)config.sht4x_sda, (gpio_num_t)config.sht4x_scl);
    int errors = 1;
    i2c_err_t res = sht.readSerial();
    sht.errorHandler(res);
    ESP_LOGI(TAG, "SHT4x serial: %u", sht.getSerial());
    while (1) {
        vTaskDelay(5 * configTICK_RATE_HZ);
        i2c_err_t res = sht.measure();
        dht_ret = (int)res;
        if (!sht.errorHandler(res)) {
            if (errors && ++errors > 3) {
                ESP_LOGI(TAG, "Stopping SHT4x task");
                break;
            }
            continue;
        }
        errors = 0;
        hum = sht.getHumidity(), temp = sht.getTemperature();
        ESP_LOGI(TAG, "Humidity %d.%d, Temp %d.%d", hum / 10, hum % 10, temp / 10, temp % 10);
        handleHumidity();
    }
    vTaskDelete(nullptr);
}

extern "C" void app_main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    uart_driver_install(UART_NUM_0, 512, 512, 32, NULL, ESP_INTR_FLAG_LEVEL1);
    // esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set("SHT4x", ESP_LOG_DEBUG);
    nvs.Init();
    config.Read();
    initRFTKey(config.rftKey);
    i2c_sniffer_init(false);
    i2c_master_init();
    i2c_slave_init(&i2c_slave_callback);
    wifi_init();
    mqtt_init();
    mqtt_on_connect(&mqtt_connect_callback);
    if (config.dht_gpio) {
        xTaskCreatePinnedToCore(&dht_task, "dht_task", 4096, NULL, 7, NULL, 0);
    } else if (config.sht4x_sda && config.sht4x_scl) {
        xTaskCreatePinnedToCore(&sht4x_task, "sht4x_task", 4096, NULL, 7, NULL, 0);
    }
    xTaskCreatePinnedToCore(requestStatusLoopTask, "statusLoopTask", 4096, NULL, 8, NULL, 1);

    printf("Press Enter to start console\n");
    while (1) {
        int len = uart_read_bytes(UART_NUM_0, (uint8_t*)buf, 1, configTICK_RATE_HZ / 5);
        if (len > 0 && *buf == '\r') {
            break;
        }
    }
    printf("\nConsole started\n");
    verbose = true;
    while (1) {
        int len = uart_read_bytes(UART_NUM_0, (uint8_t*)buf, sizeof(buf), 2);
        if (len > 0) {
            for (int i = 0; i < len; ++i) {
                if (buf[i] == '\r')
                    buf[i] = '\n';
            }
            // uart_write_bytes(UART_NUM_0, buf, len);
            printf("%.*s", len, buf);
            if (buf[0] == '\n')
                printf("\n");
            for (int i = 0; i < len; ++i) {
                if (buf[i] == '\n' || buf[i] == '\r') {
                    // if (buf[i] == '\n')
                    //     printf("\n");
                    cmd[cmdlen] = 0;
                    if (cmdlen)
                        processConsoleCommand();
                    cmdlen = 0;
                } else {
                    cmd[cmdlen++] = buf[i];
                    if (cmdlen == sizeof(cmd)) {
                        printf("Command too long\n");
                        cmdlen = 0;
                        break;
                    }
                }
            }
        }
    }
}
