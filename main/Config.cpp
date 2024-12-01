#include "Config.h"
#include "Nvs.h"
#include "console.h"
#include "mqtt.h"
#include "util.h"
#include "wifi.h"
#include <esp_log.h>
#include <esp_mac.h>
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
    for (int i = 0; i < sensors.size(); i++) {
        sensors[i].type = nvs.ReadShort(("sens_typ" + std::to_string(i)).c_str());
        sensors[i].sda = nvs.ReadShort(("sens_sda" + std::to_string(i)).c_str());
        sensors[i].scl = nvs.ReadShort(("sens_scl" + std::to_string(i)).c_str());
    }
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
    mqtt_config.credentials.client_id = mqttId.c_str();
    mqtt_config.broker.address.uri = mqtturi.c_str();
    mqtt_config.broker.verification.certificate = trimToNull(mqttServerCert.c_str());
    mqtt_config.credentials.authentication.key = trimToNull(mqttClientKey.c_str());
    mqtt_config.credentials.authentication.certificate = trimToNull(mqttClientCert.c_str());
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
    for (int i = 0; i < sensors.size(); i++) {
        nvs.WriteShort(("sens_typ" + std::to_string(i)).c_str(), sensors[i].type);
        nvs.WriteShort(("sens_sda" + std::to_string(i)).c_str(), sensors[i].sda);
        nvs.WriteShort(("sens_scl" + std::to_string(i)).c_str(), sensors[i].scl);
    }
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
    for (int i = 0; i < max_sensors; i++) {
        if (i && sensors[i - 1].type == 0) {
            sensors[i].type = 0;
        } else {
            if (!read_short(("Sensor " + std::to_string(i + 1) + " type (0=unused, 1=DHT, 2=SHT4x)").c_str(), sensors[i].type)) {
                return false;
            }
        }
        if (sensors[i].type == 0) {
            sensors[i].sda = sensors[i].scl = 0;
            continue;
        }
        if (!read_short(("Sensor " + std::to_string(i + 1) + " SDA GPIO").c_str(), sensors[i].sda)) {
            return false;
        }
        if (sensors[i].type == SensorTypeDHT) {
            sensors[i].scl = 0;
            continue;
        }
        if (!read_short(("Sensor " + std::to_string(i + 1) + " SCL GPIO").c_str(), sensors[i].scl)) {
            return false;
        }
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
