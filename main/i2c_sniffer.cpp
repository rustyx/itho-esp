#include "i2c_sniffer.h"
#include <soc/gpio_periph.h> // ESP32 GPIO
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

static const char* TAG = "i2c-sniffer";

typedef enum { ACK = 0x100, START = 0x200, STOP = 0x400 } state_t;
typedef uint16_t gpdata_t;
static QueueHandle_t gpio_evt_queue;
#define pin_(pin) GPIO_NUM_##pin
#define PIN(pin)  pin_(pin)
#if I2C_SNIFFER_SCL_PIN < 32 && I2C_SNIFFER_SDA_PIN < 32
#define SCL     (1 << I2C_SNIFFER_SCL_PIN)
#define SDA     (1 << I2C_SNIFFER_SDA_PIN)
#define GPIO_in GPIO.in
#elif I2C_SNIFFER_SCL_PIN >= 32 && I2C_SNIFFER_SDA_PIN >= 32
#define SCL     (1 << (I2C_SNIFFER_SCL_PIN - 32))
#define SDA     (1 << (I2C_SNIFFER_SDA_PIN - 32))
#define GPIO_in GPIO.in1.data
#else
#error I2C_SNIFFER_SCL_PIN and I2C_SNIFFER_SDA_PIN must be together < 32 or >= 32
#endif
static uint32_t last = SCL | SDA;
static uint32_t cur;
static uint32_t bits;
static uint32_t state;
static uint32_t tm1, tm2, tm;

static inline IRAM_ATTR void enable_sda_intr(bool en) {
    // gpio_set_intr_type() is not IRAM, i.e. too slow
    GPIO.pin[I2C_SNIFFER_SDA_PIN].int_type = en ? GPIO_INTR_ANYEDGE : GPIO_INTR_DISABLE;
}

static inline IRAM_ATTR uint32_t read_sda_scl_pins() {
    // gpio_get_level() is not IRAM, i.e. too slow
    return GPIO_in & (SCL | SDA);
}

// SCL=1, SDA 1>0 = Start
// SCL=1, SDA 0>1 = End
// SCL 0>1, SDA = data
static void IRAM_ATTR gpio_sda_handler(void* arg) {
    uint32_t st = read_sda_scl_pins();
    if (last == (SCL | SDA) && st == SCL) {
        enable_sda_intr(false);
        state = START;
        cur = bits = 0;
        tm1 = tm2 = esp_timer_get_time();
        do {
            last = st;
            st = read_sda_scl_pins();
            tm = esp_timer_get_time();
            if (last == SCL && st == (SCL | SDA)) {
                state = STOP | ((tm - tm1) << 16);
                xQueueSendFromISR(gpio_evt_queue, &state, NULL);
                break;
            } else if ((st & SCL) && !(last & SCL)) {
                if (++bits < 9)
                    cur = (cur << 1) | !!(st & SDA);
                else {
                    cur |= state | ((st & SDA) ? 0 : ACK) | ((tm - tm2) << 16);
                    xQueueSendFromISR(gpio_evt_queue, &cur, NULL);
                    state = bits = cur = 0;
                    tm2 = tm;
                }
            }
        } while (tm - tm1 < 14900);
        enable_sda_intr(true);
    }
    last = st;
}

static void sniffer_task(void* arg) {
    uint32_t x;
    uint32_t flag = 0;
    uint32_t tm1 = esp_timer_get_time(), tm;

    gpio_evt_queue = xQueueCreate(256, sizeof(uint32_t));
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
    gpio_config_t config = {BIT64(I2C_SNIFFER_SCL_PIN) | BIT64(I2C_SNIFFER_SDA_PIN), GPIO_MODE_INPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE,
                            GPIO_INTR_DISABLE};
    gpio_config(&config);
    gpio_isr_handler_add(PIN(I2C_SNIFFER_SDA_PIN), gpio_sda_handler, (void*)I2C_SNIFFER_SDA_PIN);
    ESP_LOGI(TAG, "Sniffer pin assignment: SCL=%d, SDA=%d, sniffer %s", I2C_SNIFFER_SCL_PIN, I2C_SNIFFER_SDA_PIN, arg ? "ON" : "OFF");
    enable_sda_intr(!!arg);

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &x, portMAX_DELAY)) {
            tm = esp_timer_get_time();
            if (flag && (tm - tm1) > 200000)
                printf("\n\n");
            tm1 = tm;
            flag = 1;
            if (x & START) {
                printf("[");
            }
            if (x & STOP) {
#if I2C_SNIFFER_PRINT_TIMING
                printf("]/%d\n", (x >> 16));
#else
                printf("]\n");
#endif
            } else {
#if I2C_SNIFFER_PRINT_TIMING
                printf("%02lX/%d%c", (x & 0xFF), (x >> 16), (x & ACK ? '+' : '-'));
#else
                printf("%02lX%c", (x & 0xFF), (x & ACK ? '+' : '-'));
#endif
            }
        }
    }
}

void i2c_sniffer_init(bool enabled) {
    xTaskCreatePinnedToCore(sniffer_task, "sniffer_task", 4096, (void*)enabled, 17, NULL, I2C_SNIFFER_RUN_ON_CORE);
}

void i2c_sniffer_enable() { enable_sda_intr(true); }

void i2c_sniffer_disable() {
    enable_sda_intr(false);
    vTaskDelay(1);
    enable_sda_intr(false);
}

void i2c_sniffer_pullup(bool enable) {
    if (enable) {
        gpio_pullup_en(PIN(I2C_SNIFFER_SCL_PIN));
        gpio_pullup_en(PIN(I2C_SNIFFER_SDA_PIN));
    } else {
        gpio_pullup_dis(PIN(I2C_SNIFFER_SCL_PIN));
        gpio_pullup_dis(PIN(I2C_SNIFFER_SDA_PIN));
    }
}
