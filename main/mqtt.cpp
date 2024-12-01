#include "mqtt.h"
#include "wifi.h"
#include <esp32/rom/ets_sys.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mbedtls/ssl.h>
#include <mqtt_client.h>
#include <nvs_flash.h>
#include <string.h>
#include <string>

static const char* TAG = "MQTT";
mqtt_config_t mqtt_config;
static esp_mqtt_client_handle_t mqttClient;
static EventGroupHandle_t mqtt_event_group;
const int MQTT_CONNECTED_BIT = BIT1;
static mqtt_callback_t mqttCallback;
#define MAX_MQTT_CALLBACKS 2
static mqtt_connect_callback_t mqtt_connect_callbacks[MAX_MQTT_CALLBACKS];

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    // int msg_id;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        for (int i = 0; i < MAX_MQTT_CALLBACKS; ++i) {
            if (!mqtt_connect_callbacks[i])
                break;
            mqtt_connect_callbacks[i]();
        }
        // ESP_LOGI(TAG, "Connected");
        // msg_id = esp_mqtt_client_subscribe(mqttClient, mUfo.GetConfig().msMqttTopic.c_str(), mUfo.GetConfig().muMqttQos);
        // ESP_LOGI(TAG, "Connected, subscribing to %s, msg_id=%d", mUfo.GetConfig().msMqttTopic.c_str(), msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        ESP_LOGI(TAG, "Disconnected");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribed, msg_id=%d", event->msg_id);
        // msg_id = esp_mqtt_client_publish(client, mUfo.GetConfig().msMqttTopic, "ping", 0, 0, 0);
        // ESP_LOGI(LOGTAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Message received: '%.*s'", event->data_len, event->data);
        if (mqttCallback) {
            mqttCallback(event->topic, event->topic_len, event->data, event->data_len);
        }
        // HandleMessage(String(event->data, event->data_len));
        // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        // printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        break;
    }
}

bool mqtt_wait_for_connection(TickType_t delay) {
    return !!(xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT, false, true, delay) & MQTT_CONNECTED_BIT);
}

bool mqtt_is_connected() { return !!(xEventGroupGetBits(mqtt_event_group) & MQTT_CONNECTED_BIT); }

void mqtt_on_connect(mqtt_connect_callback_t cb) {
    for (int i = 0; i < MAX_MQTT_CALLBACKS; ++i) {
        if (!mqtt_connect_callbacks[i]) {
            mqtt_connect_callbacks[i] = cb;
            return;
        }
    }
    ESP_LOGE(TAG, "mqtt_on_connect: too many callbacks");
}

static void mqtt_init_task(void* pvParameter) {
    ESP_LOGI(TAG, "Waiting for connection");
    wifi_wait_for_connection(portMAX_DELAY);
    const char* uri = strchr(mqtt_config.broker.address.uri, '@');
    ESP_LOGI(TAG, "Connecting %s to %s", mqtt_config.credentials.client_id, (uri ? uri + 1 : mqtt_config.broker.address.uri));
    mqttClient = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_register_event(mqttClient, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqttClient);
    vTaskDelete(NULL);
}

int mqtt_publish(const char* topic, const char* data) {
    if (!mqttClient)
        return 0;
    return mqtt_publish_bin(topic, data, strlen(data));
}

int mqtt_publish_bin(const char* topic, const char* data, int len) {
    if (!mqttClient)
        return 0;
    int msg_id = esp_mqtt_client_publish(mqttClient, topic, data, len, mqtt_config.pub_qos, 0);
    ESP_LOGD(TAG, "publish to %s: msg_id=%d", topic, msg_id); // msg_id > 0 only when QoS > 0
    return msg_id;
}

esp_err_t mqtt_subscribe(const char* topic, mqtt_callback_t callback) {
    if (!mqttClient)
        return 0;
    if (!mqttCallback && callback)
        mqttCallback = callback;
    esp_err_t rc = esp_mqtt_client_subscribe(mqttClient, topic, mqtt_config.sub_qos);
    ESP_LOGI(TAG, "mqtt_subscribe '%s': rc=%d", topic, rc);
    return rc;
}

void mqtt_init() {
    mqtt_event_group = xEventGroupCreate();
    if (mqtt_config.broker.address.uri && mqtt_config.broker.address.uri[0]) {
        xTaskCreatePinnedToCore(mqtt_init_task, "mqtt_init_task", 4096, NULL, 5, NULL, 0);
    }
}
