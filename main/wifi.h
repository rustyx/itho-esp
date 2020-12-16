#pragma once

#include "wifi.h"
#include <esp32/rom/ets_sys.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <string.h>
#include <string>

extern bool wifiAPMode;
extern std::string wifiSsid, wifiPw;

void wifi_init();
bool wifi_wait_for_connection(TickType_t delay = portMAX_DELAY);
