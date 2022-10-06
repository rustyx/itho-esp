#include "Config.h"
#include "Nvs.h"
#include "console.h"
#include "mqtt.h"
#include "util.h"
#include "wifi.h"
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <nvs_flash.h>

static const char* TAG = "config";

extern Nvs nvs;

bool Config::Read() {
    ESP_LOGI(TAG, "Reading config");
    if (!nvs.StartRead()) {
        ESP_LOGW(TAG, "Not configured. Enter \"config\" to configure.");
        return false;
    }
    wifiAPMode = false;
    wifiSsid = nvs.ReadString("ssid");
    wifiPw = nvs.ReadString("wifipw");
    mqtturi = nvs.ReadString("mqtturi");
    mqttId = nvs.ReadString("mqttid");
    mqttServerCert = nvs.ReadString("mqttsc");
    mqttClientKey = nvs.ReadString("mqttck");
    mqttClientCert = nvs.ReadString("mqttcc");
    mqttqos = nvs.ReadShort("mqttqos");
    rftKey = nvs.ReadInt("rftKey");
    high_hum_threshold = normalize_high_hum_threshold(nvs.ReadShort("hum1"));
    nvs.EndRead();
    uint8_t mac[8];
    int rc = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "esp_read_mac: %s", esp_err_to_name(rc));
    } else {
        char buf[8];
        sprintf(buf, "-%02x%02x%02x", mac[3], mac[4], mac[5]);
        mqttId += buf;
    }
    mqtt_config.client_id = mqttId.c_str();
    mqtt_config.uri = mqtturi.c_str();
    mqtt_config.cert_pem = trimToNull(mqttServerCert.c_str());
    mqtt_config.client_key_pem = trimToNull(mqttClientKey.c_str());
    mqtt_config.client_cert_pem = trimToNull(mqttClientCert.c_str());
    mqtt_config.pub_qos = mqttqos;
    mqtt_config.sub_qos = mqttqos;
    return true;
}

bool Config::Write() {
    nvs.StartWrite();
    nvs.WriteString("ssid", wifiSsid);
    nvs.WriteString("wifipw", wifiPw);
    nvs.WriteString("mqtturi", mqtturi);
    nvs.WriteString("mqttid", mqttId);
    nvs.WriteString("mqttsc", mqttServerCert);
    nvs.WriteString("mqttck", mqttClientKey);
    nvs.WriteString("mqttcc", mqttClientCert);
    nvs.WriteShort("mqttqos", mqttqos);
    nvs.WriteInt("rftKey", rftKey);
    nvs.WriteShort("hum1", high_hum_threshold);
    return nvs.EndWrite();
}

bool Config::Reconfigure() {
    std::string tmp;
    if (!console_read("RFT Key (4 hex bytes): ", tmp))
        return false;
    if (tmp.empty()) {
        console_read("Sniff the RFT key? (y/n): ", tmp);
        if (tmp == "y" || tmp == "Y") {
            return true;
        }
    }
    rftKey = 0;
    parseHexStr(tmp.c_str(), tmp.length(), (uint8_t*)&rftKey, 4);
    if (!console_read("SSID: ", wifiSsid))
        return false;
    if (wifiSsid.empty()) {
        ESP_LOGI(TAG, "Wi-Fi disabled");
    } else {
        if (!console_read("WiFi pw: ", wifiPw))
            return false;
    }
    if (!console_read("MQTT URI (e.g. mqtts://user:pass@host:port): ", mqtturi))
        return false;
    if (mqtturi.empty()) {
        ESP_LOGI(TAG, "MQTT disabled");
    } else {
        if (!console_read("MQTT Client ID prefix [esp]: ", mqttId, "esp"))
            return false;
        if (!console_read("MQTT Server Cert PEM (if needed): ", mqttServerCert, "", true))
            return false;
        if (!console_read("MQTT Client Key PEM (if needed): ", mqttClientKey, "", true))
            return false;
        if (mqttClientKey.empty())
            mqttClientCert = "";
        else if (!console_read("MQTT Client Cert PEM (if needed): ", mqttClientCert, "", true))
            return false;
        if (!console_read("MQTT QoS (0,1,2): ", tmp))
            return false;
        mqttqos = atoi(tmp.c_str());
    }
    std::string defaultValue = std::to_string(default_high_hum_threshold);
    std::string prompt = "high_hum_threshold? (in 0.1%) [" + defaultValue + "]";
    if (!console_read(prompt.c_str(), tmp, defaultValue.c_str()))
        return false;
    high_hum_threshold = normalize_high_hum_threshold(atoi(tmp.c_str()));
    if (!Write()) {
        ESP_LOGE(TAG, "Config write failed");
        return false;
    }
    ESP_LOGI(TAG, "Config saved. Restarting");
    vTaskDelay(configTICK_RATE_HZ);
    esp_restart();
    return false;
}
