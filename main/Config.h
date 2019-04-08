#pragma once
#include <string>

class Config {
  public:
    std::string mqtturi, mqttId, mqttServerCert, mqttClientKey, mqttClientCert;
    uint32_t rftKey;
    uint16_t mqttqos;
    bool Read();
    /* returns true if sniffing is requested, false on error. Reboots on success (never returns) */
    bool Reconfigure();
    bool Write();
};

inline const char* trimToNull(const char* s) { return s && s[0] ? s : nullptr; }
