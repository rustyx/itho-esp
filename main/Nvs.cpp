#include "Nvs.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>

static const char TAG[] = "Nvs";

void Nvs::Init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ret = nvs_flash_erase();
        ESP_LOGE(TAG, "nvs_flash_erase: %s", esp_err_to_name(ret));
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init: %s", esp_err_to_name(ret));
    }
}

bool Nvs::StartRead() {
    if (open)
        ESP_LOGE(TAG, "Nvs already open (forget to call EndRead/EndWrite?)");
    esp_err_t ret = nvs_open(configName, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open: %s", esp_err_to_name(ret));
        return false;
    }
    open = true;
    return true;
}

void Nvs::EndRead() {
    nvs_close(h);
    open = false;
}

bool Nvs::StartWrite() {
    if (open)
        ESP_LOGE(TAG, "Nvs already open (forget to call EndRead/EndWrite?)");
    esp_err_t ret = nvs_open(configName, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open: %s", esp_err_to_name(ret));
        return false;
    }
    ret = nvs_erase_all(h); // otherwise I need double the space
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "nvs_erase_all: %s", esp_err_to_name(ret));
    open = true;
    return true;
}

bool Nvs::EndWrite() {
    esp_err_t ret = nvs_commit(h);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "nvs_commit: %s", esp_err_to_name(ret));
    nvs_close(h);
    open = false;
    return ret == ESP_OK;
}

//------------------------------------------------------------------------------------

bool Nvs::ReadBool(const char* sKey, bool defaultValue) {
    uint8_t u = defaultValue;
    esp_err_t rc = nvs_get_u8(h, sKey, &u);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_u%d(%s): %s", 8, sKey, esp_err_to_name(rc));
    }
    return u;
}

uint16_t Nvs::ReadShort(const char* sKey, uint16_t defaultValue) {
    uint16_t u = defaultValue;
    esp_err_t rc = nvs_get_u16(h, sKey, &u);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_u%d(%s): %s", 16, sKey, esp_err_to_name(rc));
    }
    return u;
}

uint32_t Nvs::ReadInt(const char* sKey, uint32_t defaultValue) {
    uint32_t u = defaultValue;
    esp_err_t rc = nvs_get_u32(h, sKey, &u);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_u%d(%s): %s", 32, sKey, esp_err_to_name(rc));
    }
    return u;
}

std::string Nvs::ReadString(const char* sKey, const char* defaultValue) {
    std::string rs;
    size_t u = 0;
    esp_err_t rc = nvs_get_blob(h, sKey, nullptr, &u);
    if (u) {
        rs.resize(u);
        rc = nvs_get_blob(h, sKey, &rs[0], &u);
        if (rc != ESP_OK) {
            ESP_LOGW(TAG, "nvs_get_blob(%s): %s", sKey, esp_err_to_name(rc));
        }
    }
    return rs;
}

bool Nvs::WriteBool(const char* sKey, bool bValue) {
    esp_err_t rc = nvs_set_u8(h, sKey, bValue ? 1 : 0);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_u%d(%s): %s", 8, sKey, esp_err_to_name(rc));
        return false;
    }
    return true;
}

bool Nvs::WriteShort(const char* sKey, uint16_t iValue) {
    esp_err_t rc = nvs_set_u16(h, sKey, iValue);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_u%d(%s): %s", 16, sKey, esp_err_to_name(rc));
        return false;
    }
    return true;
}

bool Nvs::WriteInt(const char* sKey, uint32_t iValue) {
    esp_err_t rc = nvs_set_u32(h, sKey, iValue);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_u%d(%s): %s", 32, sKey, esp_err_to_name(rc));
        return false;
    }
    return true;
}

bool Nvs::WriteString(const char* sKey, const std::string& rsValue) {
    esp_err_t rc = nvs_set_blob(h, sKey, rsValue.c_str(), rsValue.length());
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_blob(%s): %s", sKey, esp_err_to_name(rc));
        return false;
    }
    return true;
}
