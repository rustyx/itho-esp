#pragma once
#include <freertos/FreeRTOS.h>
#include <mqtt_client.h>

struct mqtt_config_t : public esp_mqtt_client_config_t {
    int pub_qos;
    int sub_qos;
};

extern mqtt_config_t mqtt_config;
void mqtt_init();
bool mqtt_is_connected();
bool mqtt_wait_for_connection(TickType_t delay);
int mqtt_publish(const char* topic, const char* data);
int mqtt_publish_bin(const char* topic, const char* data, int len);
typedef void (*mqtt_callback_t)(const char* topic, int topic_len, const char* data, int data_len);
esp_err_t mqtt_subscribe(const char* topic, mqtt_callback_t callback);
typedef void (*mqtt_connect_callback_t)();
void mqtt_on_connect(mqtt_connect_callback_t);
