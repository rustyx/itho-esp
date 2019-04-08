#pragma once
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

#define DHT_IO GPIO_NUM_15

#define DHT_OK             0
#define DHT_CHECKSUM_ERROR -1
#define DHT_TIMEOUT_ERROR  -2
class DHT {
  public:
    DHT(gpio_num_t pin) { setGpio(pin); }
    void setGpio(gpio_num_t gpio);
    bool errorHandler(int response);
    int readDHT();
    int getHumidity() const { return humidity; }
    int getTemperature() const { return temperature; }

  private:
    portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
    gpio_num_t dhtGpio;
    int humidity = 0;
    int temperature = 0;
    int waitForLevel(int usTimeOut, bool state);
};
