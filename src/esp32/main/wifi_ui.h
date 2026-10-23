#pragma once
#include "lvgl.h"
#include "settings.h"
void wifi_ui_open(lv_obj_t *parent, const settings_t *settings);
bool wifi_ui_is_open(void);
