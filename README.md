
## itho-esp

Itho diag port controller

### WTF is this?

This is an ESP32 firmware that connects to the dianostic port of an Itho device in order to communicate with it and let it do things it couldn't before.

Originally designed for controlling the Itho HRU ecofan 350 ventilation unit via MQTT, but can be used to play with the I2C bus of many other Itho products.

Some key features are:

* I2C bus sniffing - dumps messages to/from the Itho CPU. On products that have peripherals like an RFT receiver, will dump traffic between the peripheral and the CPU.
* Send and receive dianostic messages to the CPU.
* Query the device status and parse parameter values.
* Control the device via Wi-Fi/MQTT.
* Monitor temperature and humidity via DHT22 or SHT4x, publish results via MQTT.
* (HRU-specific) Control ventilation level setting (low, med., high) via MQTT.
* (HRU-specific) Automatically set ventilation to high when humidity exceeds a threshold value.

### Prerequisites

Hardware:

* An Itho ecofan HRU 350 WTW unit (or another Itho product that has an RJ45 diagnostic port)
* ESP32 Dev Kit (or a raw ESP32 unit + power supply + a USB COM dongle + some SMD work)
* a logic level shifter for converting 5V <-> 3,3V logic levels (or 2 small N-channel MOSFETs)
* DHT22 or SHT4x (optional, to measure humidity)
* DC-DC buck converter (optional, to power ESP32 directly from the Itho diag port's +15V)

Software:

* [ESP-IDF Framework](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) v4.4+ and its prerequisites (`xtensa-esp32-elf-gcc`, `pip`, etc.)
* USB COM driver matching the USB serial interface chip (see [Connect ESP32 to PC](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/establish-serial-connection.html))
* [Visual Studio Code](https://code.visualstudio.com/download) with [ESP-IDF extension](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/ONBOARDING.md) (optional but recommended, will also help with installing the ESP-IDF framework)

### Building

* Make sure the ESP Dev Kit can be connected to USB and a COM port is functional
* Install ESP-IDF framework, make sure it works and has access to the COM port (see [connect your device](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/#get-started-connect))
* Build the firmware: `idf.py build`
* Flash the firmware: `idf.py -b 921600 flash` (will also invoke `build` if needed)
* Attach to the console: `idf.py monitor` (can combine with the `flash` command)

### Console interface

* `config` : Configure Wi-Fi, MQTT and humidity sensor GPIO
* `p` : Pullup OFF
* `P` : Pullup ON
* `s` : Sniffer OFF
* `S` : Sniffer ON
* `h` : Hex reporting OFF
* `H` : Hex reporting ON
* Or any of MQTT command below

### MQTT commands

* `set1` - set ventilation level to low
* `set2` - set ventilation level to medium
* `set3` - set ventilation level to high for 30 min (then back to low)
* `hum` - request DHT22 temp/humidity values
* `status` - request device status
* `ping` - request `pong`
* `high_hum_threshold` - get current high humidity threshold (output goes to `esp-data-dht`)
* `high_hum_threshold N` - set high humidity threshold to N * 0.1% (e.g. for 75% use 750)
* Hex bytes - send these bytes to the bus

### MQTT topics

`esp` - The topic for MQTT requests.

`esp-data` - The topic for MQTT replies. In addition, Itho status data + DHT data are published here as a JSON array every 5 seconds.

`esp-data-dht` - DHT data (humidity, temp, status) is published here when `hum` is requested.

`esp-data-hex` - when hex reporting is enabled, all Itho response messages are published here in hex format.

### Tech specs

For technical details and other research notes refer to [Specs.md](Specs.md)

### License

The unlicense:

The software is provided AS-IS without any warranty.

You can do whatever you want with it to the extent permitted by law.

I will deny any involvement with this project, should any questions arise.
