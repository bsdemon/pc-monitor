#include "network.h"
#include "metrics.h"
#include <stdlib.h>
#include <string.h>
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define CONNECTED BIT0
#define SCAN_REQUEST BIT1
#define SCANNING BIT2
#define RESPONSE_LIMIT (16 * 1024)
static EventGroupHandle_t events;
static settings_t config;
static bool configured;
static QueueHandle_t samples, scans;

bool network_connected(void) { return events && (xEventGroupGetBits(events) & CONNECTED); }
bool network_scan_busy(void) { return events && (xEventGroupGetBits(events) & SCANNING); }
void network_request_scan(void)
{
    if (events && !network_scan_busy()) xEventGroupSetBits(events, SCAN_REQUEST | SCANNING);
}
bool network_get_scan(wifi_scan_t *result) { return scans && xQueuePeek(scans, result, 0) == pdTRUE; }

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(events, CONNECTED);
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) xEventGroupClearBits(events, CONNECTED);
}

static void scan_networks(void)
{
    wifi_scan_t result = {0};
    if (!network_connected()) {
        /* Cancel a pending join attempt so station scanning can proceed. */
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    wifi_scan_config_t scan = {.show_hidden = false};
    result.error = esp_wifi_scan_start(&scan, true);
    if (result.error == ESP_OK) {
        wifi_ap_record_t records[WIFI_SCAN_LIMIT];
        uint16_t count = WIFI_SCAN_LIMIT;
        result.error = esp_wifi_scan_get_ap_records(&count, records);
        if (result.error == ESP_OK) for (int i = 0; i < count; i++) {
            char ssid[33]; memcpy(ssid, records[i].ssid, 32); ssid[32] = 0;
            if (!ssid[0]) continue;
            bool duplicate = false;
            for (int j = 0; j < result.count; j++) if (!strcmp(ssid, result.networks[j].ssid)) duplicate = true;
            if (duplicate) continue;
            wifi_network_t *n = &result.networks[result.count++];
            memcpy(n->ssid, ssid, sizeof(ssid));
            n->rssi = records[i].rssi;
            n->open = records[i].authmode == WIFI_AUTH_OPEN;
        }
    }
    xQueueOverwrite(scans, &result);
    xEventGroupClearBits(events, SCANNING);
}
static bool fetch_metrics(metrics_t *out, char *body)
{
    esp_http_client_config_t cfg = {
        .url = config.url, .timeout_ms = 2500, .disable_auto_redirect = true,
        .buffer_size = 2048, .buffer_size_tx = 512,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    bool ok = false;
    if (esp_http_client_open(client, 0) != ESP_OK) goto done;
    int64_t size = esp_http_client_fetch_headers(client);
    if (size < 0 || size > RESPONSE_LIMIT || esp_http_client_get_status_code(client) != 200) goto done;
    int used = 0;
    while (used < RESPONSE_LIMIT) {
        int got = esp_http_client_read(client, body + used, RESPONSE_LIMIT - used);
        if (got < 0) goto done;
        if (got == 0) break;
        used += got;
    }
    if (!esp_http_client_is_complete_data_received(client) || used >= RESPONSE_LIMIT) goto done;
    body[used] = 0;
    ok = metrics_parse(body, used + 1, out);
 done:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ok;
}

static void network_task(void *unused)
{
    char *body = malloc(RESPONSE_LIMIT + 1);
    ESP_ERROR_CHECK(body ? ESP_OK : ESP_ERR_NO_MEM);
    int64_t last_attempt = -5000000;
    for (;;) {
        int64_t start = esp_timer_get_time();
        if (xEventGroupGetBits(events) & SCAN_REQUEST) {
            xEventGroupClearBits(events, SCAN_REQUEST);
            scan_networks();
        }
        if (network_connected()) {
            metrics_t m;
            if (fetch_metrics(&m, body)) xQueueOverwrite(samples, &m);
        } else if (configured && start - last_attempt >= 5000000) {
            esp_wifi_connect();
            last_attempt = start;
        }
        int elapsed_ms = (esp_timer_get_time() - start) / 1000;
        vTaskDelay(pdMS_TO_TICKS(elapsed_ms < config.interval_ms ? config.interval_ms - elapsed_ms : 20));
    }
}

void network_start(const settings_t *settings, bool has_config, QueueHandle_t queue)
{
    config = *settings; configured = has_config; samples = queue;
    events = xEventGroupCreate();
    scans = xQueueCreate(1, sizeof(wifi_scan_t));
    ESP_ERROR_CHECK(events && scans ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (configured) {
        wifi_config_t sta = {0};
        memcpy(sta.sta.ssid, config.ssid, strlen(config.ssid));
        memcpy(sta.sta.password, config.password, strlen(config.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(xTaskCreate(network_task, "metrics_http", 12288, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
