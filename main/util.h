#pragma once
#include <cstddef>
#include <cstdint>

inline bool isHex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

inline char parseHex(char c) { return c <= '9' ? c - '0' : (c >= 'a' ? c - 'a' + 10 : c - 'A' + 10); }

size_t parseHexStr(const char* hex, size_t hexlen, uint8_t* buf, size_t buflen);

inline uint8_t checksum(const uint8_t* buf, size_t buflen) {
    uint8_t sum = 0;
    while (buflen--) {
        sum += *buf++;
    }
    return -sum;
}

inline void fixChecksum(uint8_t* buf, size_t buflen) { buf[buflen - 1] = checksum(buf, buflen - 1); }

inline char toHex(uint8_t c) { return c < 10 ? c + '0' : c + 'A' - 10; }
