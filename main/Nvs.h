#pragma once

#include <nvs.h>
#include <string>

class Nvs {
  public:
    Nvs(const char* configName = "cfg") : configName(configName) {}

    void Init();
    bool StartRead();
    bool ReadBool(const char* sKey, bool defaultValue = false);
    uint16_t ReadShort(const char* sKey, uint16_t defaultValue = 0);
    uint32_t ReadInt(const char* sKey, uint32_t defaultValue = 0);
    std::string ReadString(const char* sKey, const char* defaultValue = "");
    void EndRead();

    bool StartWrite();
    bool WriteBool(const char* sKey, bool bValue);
    bool WriteShort(const char* sKey, uint16_t bValue);
    bool WriteInt(const char* sKey, uint32_t bValue);
    bool WriteString(const char* sKey, const std::string& rsValue);
    bool EndWrite();

  private:
    const char* configName;
    nvs_handle h = {};
    bool open = false;
};
