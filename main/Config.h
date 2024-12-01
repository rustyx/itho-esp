#pragma once
#include <array>
#include <string>

constexpr int max_sensors = 3;

constexpr uint16_t SensorTypeDHT = 1;
constexpr uint16_t SensorTypeSHT4x = 2;

class SensorConfig {
  public:
    uint16_t type = 0;
    uint16_t sda = 0;
    uint16_t scl = 0;
};

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
    std::array<SensorConfig, max_sensors> sensors;

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
