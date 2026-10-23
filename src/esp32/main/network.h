#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "settings.h"

#define WIFI_SCAN_LIMIT 12
typedef struct { char ssid[33]; int rssi; bool open; } wifi_network_t;
typedef struct { int count; esp_err_t error; wifi_network_t networks[WIFI_SCAN_LIMIT]; } wifi_scan_t;

void network_start(const settings_t *settings, bool configured, QueueHandle_t queue);
bool network_connected(void);
void network_request_scan(void);
bool network_scan_busy(void);
bool network_get_scan(wifi_scan_t *result);
