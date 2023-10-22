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
    dht_gpio = nvs.ReadShort("dht_gpio");
    sht4x_sda = nvs.ReadShort("sht4x_sda");
    sht4x_scl = nvs.ReadShort("sht4x_scl");
    nvs.EndRead();
    if (mqttId.empty()) {
        uint8_t mac[8];
        int rc = esp_read_mac(mac, ESP_MAC_WIFI_STA);
        if (rc != ESP_OK) {
            ESP_LOGE(TAG, "esp_read_mac: %s", esp_err_to_name(rc));
            std::fill_n(mac, sizeof(mac), 0);
        }
        char buf[16];
        sprintf(buf, "esp-%02x%02x%02x", mac[3], mac[4], mac[5]);
        mqttId = buf;
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
    nvs.WriteShort("dht_gpio", dht_gpio);
    nvs.WriteShort("sht4x_sda", sht4x_sda);
    nvs.WriteShort("sht4x_scl", sht4x_scl);
    return nvs.EndWrite();
}

int Config::read_string(const char* msg, std::string& val, bool multiline) {
    std::string prompt = msg;
    std::string tmp;
    prompt += " [";
    if (!multiline) {
        prompt += val;
    } else if (!val.empty()) {
        prompt += "*";
    }
    prompt += "]: ";
    auto rc = console_read(prompt.c_str(), tmp, val.c_str(), multiline);
    if (rc && tmp != "") {
        val = tmp == "-" || tmp == "-\n" ? "" : tmp;
    }

    return rc;
}

int Config::read_short(const char* msg, uint16_t& val) {
    std::string defaultValue = std::to_string(val);
    std::string prompt = msg;
    std::string tmp;
    prompt += " [";
    prompt += defaultValue;
    prompt += "]: ";
    auto rc = console_read(prompt.c_str(), tmp, defaultValue.c_str());
    if (rc) {
        val = atoi(tmp.c_str());
    }

    return rc;
}

bool Config::Reconfigure() {
    std::string tmp = toHexStr((uint8_t*)&rftKey, 4);
    if (!read_string("RFT Key (4 hex bytes)", tmp))
        return false;
    rftKey = 0;
    parseHexStr(tmp.c_str(), tmp.length(), (uint8_t*)&rftKey, 4);
    if (rftKey == 0) {
        if (!read_string("Sniff the RFT key? (y/n): ", tmp))
            return false;
        if (tmp == "y" || tmp == "Y") {
            return true;
        }
    }
    if (!read_string("SSID", wifiSsid))
        return false;
    if (wifiSsid.empty()) {
        ESP_LOGI(TAG, "Wi-Fi disabled");
    } else {
        if (!read_string("WiFi pw", wifiPw))
            return false;
    }
    if (!read_string("MQTT URI (e.g. mqtts://user:pass@host:port)", mqtturi))
        return false;
    if (mqtturi.empty()) {
        ESP_LOGI(TAG, "MQTT disabled");
    } else {
        if (!read_string("MQTT Client ID", mqttId))
            return false;
        if (!read_string("MQTT Server Cert PEM (if needed)", mqttServerCert, true))
            return false;
        if (!read_string("MQTT Client Key PEM (if needed)", mqttClientKey, true))
            return false;
        if (mqttClientKey.empty())
            mqttClientCert = "";
        else if (!read_string("MQTT Client Cert PEM (if needed)", mqttClientCert, true))
            return false;
        if (!read_short("MQTT QoS (0,1,2)", mqttqos))
            return false;
    }
    if (!read_short("high_hum_threshold? (in 0.1%)", high_hum_threshold))
        return false;
    high_hum_threshold = normalize_high_hum_threshold(high_hum_threshold);
    if (!read_short("DHT GPIO, e.g. 15 (0=unused)", dht_gpio))
        return false;
    if (!read_short("SHT4x SDA GPIO (0=unused)", sht4x_sda))
        return false;
    if (sht4x_sda) {
        if (!read_short("SHT4x SCL GPIO", sht4x_scl))
            return false;
    }
    if (!Write()) {
        ESP_LOGE(TAG, "Config write failed");
        return false;
    }
    ESP_LOGI(TAG, "Config saved. Restarting");
    vTaskDelay(configTICK_RATE_HZ);
    esp_restart();
    return false;
}
