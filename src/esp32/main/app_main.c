#include "metrics.h"
#include "network.h"
#include "settings.h"
#include "ui.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(settings_import_build());
    settings_t settings;
    bool configured = settings_load(&settings);
    QueueHandle_t queue = xQueueCreate(1, sizeof(metrics_t));
    ESP_ERROR_CHECK(queue ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(bsp_display_start() ? ESP_OK : ESP_FAIL);
    network_start(&settings, configured, queue);
    ui_start(queue, &settings);
    /* This board has no controllable backlight GPIO; UI brightness uses software dimming. */
    bsp_display_backlight_on();
    ESP_LOGI("pc_monitor", "Display started. Tap the gear icon to open Settings.");
}
