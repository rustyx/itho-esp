/*------------------------------------------------------------------------------
    DHT22 CPP temperature & humidity sensor AM2302 (DHT22) driver for ESP32
    Jun 2017: Ricardo Timmermann, new for DHT22
        Timing model entirely rewritten by rustyx

    This example code is in the Public Domain (or CC0 licensed, at your option.)
    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
    PLEASE KEEP THIS CODE IN LESS THAN 0XFF LINES. EACH LINE MAY CONTAIN ONE BUG !!!
---------------------------------------------------------------------------------*/

#include "dht.h"
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdint.h>
#include <sys/types.h>

static char TAG[] = "DHT";

DHT::DHT(gpio_num_t pin) : dhtGpio(pin) {
    ESP_LOGI(TAG, "Starting DHT on GPIO %d", dhtGpio);
    gpio_config_t config{BIT64(dhtGpio), GPIO_MODE_INPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE};
    gpio_config(&config);
}

bool DHT::errorHandler(int response) {
    switch (response) {
    case DHT_TIMEOUT_ERROR:
        ESP_LOGE(TAG, "Sensor timeout");
        break;
    case DHT_CHECKSUM_ERROR:
        ESP_LOGE(TAG, "Checksum error");
        break;
    case DHT_OK:
        break;
    default:
        ESP_LOGE(TAG, "Unknown error");
    }
    return response == DHT_OK;
}

inline int DHT::waitForLevel(int usTimeOut, bool state) {
    int uSec = 0;
    while (gpio_get_level(dhtGpio) != state) {
        if (uSec > usTimeOut) {
            uSec = -1;
            break;
        }
        ++uSec;
        ets_delay_us(1); // uSec delay
    }
    return uSec;
}

/*----------------------------------------------------------------------------
;
;    read DHT22 sensor

    copy/paste from AM2302/DHT22 Docu:
    DATA: Hum = 16 bits, Temp = 16 Bits, check-sum = 8 Bits
    Example: MCU has received 40 bits data from AM2302 as
    0000 0010 1000 1100 0000 0001 0101 1111 1110 1110
    16 bits RH data + 16 bits T data + check sum

    1) we convert 16 bits RH data from binary system to decimal system, 0000 0010 1000 1100 → 652
    Binary system Decimal system: RH=652/10=65.2%RH
    2) we convert 16 bits T data from binary system to decimal system, 0000 0001 0101 1111 → 351
    Binary system Decimal system: T=351/10=35.1°C
    When highest bit of temperature is 1, it means the temperature is below 0 degree Celsius.
    Example: 1000 0000 0110 0101, T= minus 10.1°C: 16 bits T data
    3) Check Sum=0000 0010+1000 1100+0000 0001+0101 1111=1110 1110 Check-sum=the last 8 bits of Sum=11101110

    Signal & Timings:
    The interval of whole process must be beyond 2 seconds.

    To request data from DHT:
    1) Sent low pulse for > 1~10 ms (MILI SEC)
    2) Sent high pulse for > 20~40 us (Micros).
    3) When DHT detects the start signal, it will pull low the bus 80us as response signal,
       then the DHT pulls up 80us for preparation to send data.
    4) When DHT is sending data to MCU, every bit's transmission begin with low-voltage-level that last 50us,
       the following high-voltage-level signal's length decide the bit is "1" or "0".
        0: 26~28 us
        1: 70 us
;----------------------------------------------------------------------------*/

#define MAXdhtData 5 // to complete 40 = 5*8 Bits

int DHT::readDHT() {
    int uSec = 0;
    uint8_t dhtData[MAXdhtData] = {};
    uint8_t byteInx = 0;
    uint8_t bitInx = 7;

    // == Send start signal to DHT sensor ===========

    gpio_set_direction(dhtGpio, GPIO_MODE_OUTPUT);

    // pull down for 5 ms for a smooth and nice wake up
    gpio_set_level(dhtGpio, 0);
    ets_delay_us(5000);

    portENTER_CRITICAL(&mutex);
    // pull up for 25 us to gently ask for data
    gpio_set_level(dhtGpio, 1);
    ets_delay_us(25);

    gpio_set_direction(dhtGpio, GPIO_MODE_INPUT);

    // == DHT will keep the line low for 80 us and then high for 80us ====

    int uSec1 = waitForLevel(50, 0);
    if (uSec1 < 0) {
        portEXIT_CRITICAL(&mutex);
        return DHT_TIMEOUT_ERROR;
    }

    // -- 80us up ------------------------

    int uSec2 = waitForLevel(100, 1);
    // ESP_LOGD(TAG, "Response 2 = %d", uSec2);
    if (uSec2 < 0) {
        portEXIT_CRITICAL(&mutex);
        // ESP_LOGW(TAG, "Sensor timeout 2");
        return DHT_TIMEOUT_ERROR;
    }

    // == No errors, read the 40 data bits ================

    for (int k = 0; k < 40; k++) {
        // -- starts new data transmission with low signal
        uSec = waitForLevel(100, 0);
        if (uSec < 0) {
            portEXIT_CRITICAL(&mutex);
            ESP_LOGW(TAG, "Sensor timeout %d @ %d", 1, k);
            return DHT_TIMEOUT_ERROR;
        }
        // high after 50us
        uSec = waitForLevel(75, 1);
        if (uSec < 0) {
            portEXIT_CRITICAL(&mutex);
            ESP_LOGW(TAG, "Sensor timeout %d @ %d", 2, k);
            return DHT_TIMEOUT_ERROR;
        }
        // data bit after 26..28 us
        ets_delay_us(40);

        if (gpio_get_level(dhtGpio))
            dhtData[byteInx] |= (1 << bitInx);

        // index to next byte
        if (bitInx == 0) {
            bitInx = 7;
            ++byteInx;
        } else {
            --bitInx;
        }
    }
    portEXIT_CRITICAL(&mutex);

    // == get humidity from Data[0] and Data[1] ==========================
    humidity = (((unsigned)dhtData[0] << 8) | dhtData[1]);

    // == get temp from Data[2] and Data[3]
    temperature = ((((unsigned)dhtData[2] & 0x7F) << 8) | dhtData[3]);

    if (dhtData[2] & 0x80) // negative temp
        temperature = -temperature;

    ESP_LOGD(TAG, "DHT Response = %d, %d, %02x %02x %02x %02x %02x", uSec1, uSec2, dhtData[0], dhtData[1], dhtData[2], dhtData[3], dhtData[4]);

    // == verify if checksum is ok ===========================================
    // Checksum is the sum of Data 8 bits masked out 0xFF

    if (dhtData[4] == ((dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3]) & 0xFF))
        return DHT_OK;
    else
        return DHT_CHECKSUM_ERROR;
}
