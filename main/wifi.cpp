#include "wifi.h"
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <rom/ets_sys.h>
#include <string.h>

static const char* TAG = "wifi";

std::string wifiSsid, wifiPw;
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static esp_err_t event_handler(void* ctx, system_event_t* event) {
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip: %s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station: " MACSTR " join, AID=%d", MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "station: " MACSTR " leave, AID=%d", MAC2STR(event->event_info.sta_disconnected.mac), event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_softap() {
    wifi_config_t wifi_config = {.ap = {{}, {}, 0, 0, wifiPw.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK, 0, 32, 300}};
    strncpy((char*)wifi_config.ap.ssid, wifiSsid.c_str(), sizeof(wifi_config.ap.ssid));
    strncpy((char*)wifi_config.ap.password, wifiPw.c_str(), sizeof(wifi_config.ap.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_softap SSID: %s password: %s", wifiSsid.c_str(), wifiPw.c_str());
}

void wifi_init_sta() {
    wifi_config_t wifi_config = {.sta = {{}, {}, {}, false, {}, 0, 0, WIFI_CONNECT_AP_BY_SIGNAL, {}}};
    strncpy((char*)wifi_config.sta.ssid, wifiSsid.c_str(), sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, wifiPw.c_str(), sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta connecting to SSID: %s", wifiSsid.c_str());
}

bool wifi_wait_for_connection(TickType_t delay) {
    return !!(xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, delay) & WIFI_CONNECTED_BIT);
}

void wifi_init() {
    wifi_event_group = xEventGroupCreate();
    if (wifiSsid.empty())
        return;
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
#if CONFIG_ESP_WIFI_MODE_AP
    wifi_init_softap();
#else
    wifi_init_sta();
#endif
}
