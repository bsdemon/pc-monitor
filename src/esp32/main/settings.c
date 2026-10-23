#include "settings.h"
#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "local_settings.h"

void settings_defaults(settings_t *s)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->url, sizeof(s->url), "http://192.168.1.100:8080/api/metrics");
    s->interval_ms = 1000;
    s->brightness = 80;
}

bool settings_valid(const settings_t *s)
{
    size_t pw = strlen(s->password);
    return s->ssid[0] && (pw == 0 || (pw >= 8 && pw <= 63)) &&
        !strncmp(s->url, "http://", 7) && s->url[7] && !strpbrk(s->url, "\r\n\t @#") &&
        s->interval_ms >= 500 && s->interval_ms <= 10000 &&
        s->brightness >= 10 && s->brightness <= 100 && s->home_view <= 2;
}

bool settings_load(settings_t *s)
{
    settings_defaults(s);
    nvs_handle_t nvs;
    if (nvs_open("pc_monitor", NVS_READONLY, &nvs) != ESP_OK) return false;
    size_t size = sizeof(*s);
    esp_err_t err = nvs_get_blob(nvs, "config_v1", s, &size);
    nvs_close(nvs);
    if (err != ESP_OK || size != sizeof(*s) || !memchr(s->ssid, 0, sizeof(s->ssid)) ||
        !memchr(s->password, 0, sizeof(s->password)) || !memchr(s->url, 0, sizeof(s->url)) ||
        !settings_valid(s)) {
        settings_defaults(s);
        return false;
    }
    return true;
}

esp_err_t settings_save(const settings_t *s)
{
    if (!settings_valid(s)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("pc_monitor", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(nvs, "config_v1", s, sizeof(*s));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

/* Import a changed local configuration once; preserve later touch edits on normal boots. */
esp_err_t settings_import_build(void)
{
    if (!LOCAL_SETTINGS_AVAILABLE) return ESP_OK;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("pc_monitor", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    char hash[65] = {0};
    size_t length = sizeof(hash);
    settings_t existing;
    if (nvs_get_str(nvs, "build_hash", hash, &length) == ESP_OK &&
        !strcmp(hash, LOCAL_SETTINGS_HASH) && settings_load(&existing)) {
        nvs_close(nvs);
        return ESP_OK;
    }
    settings_t next;
    settings_defaults(&next);
    snprintf(next.ssid, sizeof(next.ssid), "%s", LOCAL_SETTINGS_SSID);
    snprintf(next.password, sizeof(next.password), "%s", LOCAL_SETTINGS_PASSWORD);
    snprintf(next.url, sizeof(next.url), "%s", LOCAL_SETTINGS_URL);
    next.interval_ms = LOCAL_SETTINGS_INTERVAL_MS;
    next.brightness = LOCAL_SETTINGS_BRIGHTNESS;
    next.home_view = LOCAL_SETTINGS_HOME_VIEW;
    if (!settings_valid(&next)) { nvs_close(nvs); return ESP_ERR_INVALID_ARG; }
    err = nvs_set_blob(nvs, "config_v1", &next, sizeof(next));
    if (err == ESP_OK) err = nvs_set_str(nvs, "build_hash", LOCAL_SETTINGS_HASH);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}
