#include "util.h"

size_t parseHexStr(const char* hex, size_t hexlen, uint8_t* buf, size_t buflen) {
    size_t len = 0;
    for (size_t i = 0; i < hexlen && len <= buflen; ++i) {
        if (!isHex(hex[i]))
            continue;
        if (!isHex(hex[i + 1])) {
            break;
        }
        buf[len++] = parseHex(hex[i]) << 4 | parseHex(hex[i + 1]);
        ++i;
    }
    return len;
}

std::string toHexStr(const uint8_t* data, unsigned len) {
    std::string s;
    s.reserve(len * 3 + 2);
    for (size_t i = 0; i < len; ++i) {
        if (i)
            s += ' ';
        s += toHex(data[i] >> 4);
        s += toHex(data[i] & 0xF);
    }
    return s;
}