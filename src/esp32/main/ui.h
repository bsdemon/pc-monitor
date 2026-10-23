#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "settings.h"
void ui_start(QueueHandle_t queue, const settings_t *settings);
