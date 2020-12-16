#include "wifi.h"

static const char* TAG = "WIFI";

bool wifiAPMode;
std::string wifiSsid, wifiPw;
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static void event_handler_ap(void* arg, esp_event_base_t base, int32_t event_id, void* data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

static void wifi_init_softap() {
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler_ap, NULL, NULL));
    wifi_config_t wifi_config = {.ap = {{}, {}, 0, 0, wifiPw.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK, 0, 32, 300}};
    strncpy((char*)wifi_config.ap.ssid, wifiSsid.c_str(), sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char*)wifi_config.ap.password, wifiPw.c_str(), sizeof(wifi_config.ap.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_softap SSID: %s password: %s", wifiSsid.c_str(), wifiPw.c_str());
}

static void event_handler_sta(void* arg, esp_event_base_t base, int32_t event_id, void* data) {
    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "starting");
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "disconnected");
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    }
}

static void event_handler_sta_ip(void* arg, esp_event_base_t base, int32_t event_id, void* data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta() {
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler_sta, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler_sta_ip, NULL, NULL));
    wifi_config_t wifi_config = {.sta = {{}, {}, {}, false, {}, 0, 0, WIFI_CONNECT_AP_BY_SIGNAL, {}, {}}};
    strncpy((char*)wifi_config.sta.ssid, wifiSsid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, wifiPw.c_str(), sizeof(wifi_config.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta connecting to SSID: %s", wifiSsid.c_str());
}

bool wifi_wait_for_connection(TickType_t delay) {
    return !!(xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, delay) & WIFI_CONNECTED_BIT);
}

void wifi_init() {
    esp_log_level_set("wifi", ESP_LOG_WARN); // reduce built-in wifi logging
    wifi_event_group = xEventGroupCreate();
    if (wifiSsid.empty())
        return;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (wifiAPMode) {
        wifi_init_softap();
    } else {
        wifi_init_sta();
    }
}
