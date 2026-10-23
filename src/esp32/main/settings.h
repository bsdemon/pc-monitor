#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    char ssid[33];
    char password[65];
    char url[256];
    uint16_t interval_ms;
    uint8_t brightness;
    uint8_t home_view;
} settings_t;

esp_err_t settings_import_build(void);
void settings_defaults(settings_t *out);
bool settings_load(settings_t *out);
esp_err_t settings_save(const settings_t *settings);
bool settings_valid(const settings_t *settings);
