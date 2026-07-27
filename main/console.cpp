#include "console.h"
#include <driver/uart.h>

bool console_read(const char* prompt, std::string& res, const char* defaultValue, bool multiline) {
    std::string buf;
    char c;
    bool rc = true;
    // printf("%s", prompt);
    uart_write_bytes(UART_NUM_0, prompt, strlen(prompt));
    // fflush(stdout);
    res.clear();
    while (1) {
        while (1) {
            int len = uart_read_bytes(UART_NUM_0, (uint8_t*)&c, 1, 2);
            if (len) {
                if (c < 32 && c != 9) {
                    if (c == '\n') {
                        continue;
                    } else if (c == '\r') {
                        break;
                    } else if (c == '\b') {
                        if (buf.size()) {
                            buf.resize(buf.size() - 1);
                            uart_write_bytes(UART_NUM_0, &c, 1);
                        }
                    } else {
                        res.clear();
                        rc = false;
                        break;
                    }
                } else {
                    buf += c;
                    uart_write_bytes(UART_NUM_0, &c, 1);
                    // fflush(stdout);
                }
            }
        }
        // printf("\n\n");
        uart_write_bytes(UART_NUM_0, "\n", 1);
        res += buf;
        if (multiline && !buf.empty()) {
            res += '\n';
            buf.clear();
        } else {
            break;
        }
    }
    if (res.empty())
        res = defaultValue;
    return rc;
}
