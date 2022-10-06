#pragma once
#include <string>

class Config {
  public:
    std::string mqtturi, mqttId, mqttServerCert, mqttClientKey, mqttClientCert;
    uint32_t rftKey;
    uint16_t mqttqos;
    uint16_t high_hum_threshold;
    bool Read();
    /* returns true if sniffing is requested, false on error. Reboots on success (never returns) */
    bool Reconfigure();
    bool Write();

    static constexpr uint16_t default_high_hum_threshold = 770; // * 0.1 %
    static uint16_t normalize_high_hum_threshold(uint16_t val) { return val > 0 && val <= 1000 ? val : default_high_hum_threshold; }
};

inline const char* trimToNull(const char* s) { return s && s[0] ? s : nullptr; }
