#pragma once
#include <string>

class Config {
  public:
    std::string mqtturi;
    std::string mqttId;
    std::string mqttServerCert;
    std::string mqttClientKey;
    std::string mqttClientCert;
    uint32_t rftKey = 0;
    uint16_t mqttqos = 0;
    uint16_t high_hum_threshold = default_high_hum_threshold;
    uint16_t dht_gpio = 0;
    uint16_t sht4x_sda = 0;
    uint16_t sht4x_scl = 0;
    bool Read();
    /* returns true if sniffing is requested, false on error. Reboots on success (never returns) */
    bool Reconfigure();
    bool Write();

    static constexpr uint16_t default_high_hum_threshold = 770; // * 0.1 %
    static uint16_t normalize_high_hum_threshold(uint16_t val) { return val > 0 && val <= 1000 ? val : default_high_hum_threshold; }
    int read_string(const char* msg, std::string& val, bool multiline = false);
    int read_short(const char* msg, uint16_t& val);
};

inline const char* trimToNull(const char* s) { return s && s[0] ? s : nullptr; }
