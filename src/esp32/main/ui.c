#include "ui.h"
#include "metrics.h"
#include "network.h"
#include "wifi_ui.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "esp_timer.h"
#include "lvgl.h"

#define HISTORY 120
#define GIB 1073741824.0
#define BG 0x101417
#define PANEL 0x1b2227
#define LINE 0x303c43
#define TEXT 0xf3f6f8
#define MUTED 0x9fadb5
static const uint32_t accent[] = {0xbcee83, 0x91b8ff, 0xeac18a, 0x75ddd1, 0xd6a3ff};
typedef enum { OVERVIEW, CHARTS, NETWORK, DETAIL } view_t;
static view_t view, home;
static int detail_device;
static metrics_t metrics;
static QueueHandle_t queue;
static settings_t prefs;
static bool has_sample, sample_pending;
static int64_t last_http, last_change, last_history;
static lv_obj_t *root, *content, *dot, *pager[3], *dimmer;
static lv_obj_t *names[2], *temperatures[2], *loads[2], *powers[2], *secondary[2], *bars[4];
static lv_obj_t *ram_value, *ram_load, *ram_temps, *upload_value, *download_value, *ping_value;
static lv_obj_t *stat_values[4], *extra_values[4], *core_values[32], *core_bars[32];
static lv_obj_t *chart_objs[4], *chart_values[4];
static lv_chart_series_t *chart_series[4];
static int chart_devices[4], chart_types[4], chart_count, detail_cores;
static lv_coord_t history[5][2][HISTORY];
static void build_view(void);
static void update_values(void);
static void show_settings(void);

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, const char *value,
                       const lv_font_t *font, uint32_t color)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_label_set_long_mode(o, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_text(o, value);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_GESTURE_BUBBLE);
    return o;
}

static lv_obj_t *card(int x, int y, int w, int h)
{
    lv_obj_t *o = box(content, x, y, w, h, PANEL);
    lv_obj_set_style_radius(o, 12, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(LINE), 0);
    return o;
}

static lv_obj_t *button(lv_obj_t *parent, int x, int y, int w, int h, const char *text,
                        lv_event_cb_t callback, intptr_t arg)
{
    lv_obj_t *o = box(parent, x, y, w, h, LINE);
    lv_obj_set_style_radius(o, 7, 0);
    lv_obj_add_event_cb(o, callback, LV_EVENT_CLICKED, (void *)arg);
    lv_obj_t *l = label(o, 0, 0, w, text, &lv_font_montserrat_16, TEXT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    return o;
}

static void value(lv_obj_t *o, double v, const char *suffix, int decimals)
{
    if (!o) return;
    if (isfinite(v)) lv_label_set_text_fmt(o, "%.*f%s", decimals, v, suffix);
    else lv_label_set_text(o, "N/A");
}

static void pair(lv_obj_t *o, double used, double total)
{
    char a[24] = "N/A", b[24] = "N/A";
    if (isfinite(used)) snprintf(a, sizeof(a), "%.1f", used / GIB);
    if (isfinite(total)) snprintf(b, sizeof(b), "%.1f", total / GIB);
    lv_label_set_text_fmt(o, "%s / %s GiB", a, b);
}

static lv_obj_t *bar(lv_obj_t *parent, int x, int y, int w, int h, int device)
{
    lv_obj_t *o = lv_bar_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_bar_set_range(o, 0, 100);
    lv_obj_set_style_bg_color(o, lv_color_hex(LINE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(o, h / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(o, lv_color_hex(accent[device]), LV_PART_INDICATOR);
    lv_obj_set_style_radius(o, h / 2, LV_PART_INDICATOR);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_GESTURE_BUBBLE);
    return o;
}

static void set_bar(lv_obj_t *o, float v)
{
    if (o) lv_bar_set_value(o, isfinite(v) ? (int)fminf(100, fmaxf(0, v)) : 0, LV_ANIM_OFF);
}

static void change_home(lv_event_t *e)
{
    home = (view_t)(intptr_t)lv_event_get_user_data(e);
    view = home;
    build_view();
}

static void open_device(lv_event_t *e)
{
    detail_device = (int)(intptr_t)lv_event_get_user_data(e);
    view = DETAIL;
    build_view();
}

static void back(lv_event_t *e) { view = home; build_view(); }

static void clickable(lv_obj_t *o, int device)
{
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o, open_device, LV_EVENT_CLICKED, (void *)(intptr_t)device);
}

static void speed_axis(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc && dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_Y && dsc->text)
        snprintf(dsc->text, dsc->text_length, "%.1f", dsc->value / 10.0);
}

static void make_chart(lv_obj_t *parent, int x, int y, int w, int h, int device, int type)
{
    int n = chart_count++;
    chart_devices[n] = device;
    chart_types[n] = type;
    const char *caption = device == 3 ? (type ? "Download / Mbps" : "Upload / Mbps") :
        device == 4 ? "Ping / google.com / ms" :
        type ? (device == 2 ? "Hottest DIMM / C" : "Temperature / C") : "Utilization / %";
    label(parent, x, y, w - 150, caption,
          &lv_font_montserrat_14, MUTED);
    chart_values[n] = label(parent, x + w - 150, y - 2, 150, "N/A", &lv_font_montserrat_20, TEXT);
    lv_obj_set_style_text_align(chart_values[n], LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *c = lv_chart_create(parent);
    chart_objs[n] = c;
    lv_obj_set_pos(c, x + 22, y + 30);
    lv_obj_set_size(c, w - 24, h - 51);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_style_line_color(c, lv_color_hex(LINE), LV_PART_MAIN);
    lv_obj_set_style_line_width(c, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(c, 0, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(c, &lv_font_montserrat_14, LV_PART_TICKS);
    lv_obj_set_style_text_color(c, lv_color_hex(MUTED), LV_PART_TICKS);
    if (device == 3) lv_obj_add_event_cb(c, speed_axis, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_chart_set_type(c, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(c, HISTORY);
    lv_chart_set_range(c, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(c, 3, 0);
    lv_chart_set_axis_tick(c, LV_CHART_AXIS_PRIMARY_Y, 0, 0, 2, 1, true, 26);
    chart_series[n] = lv_chart_add_series(c, lv_color_hex(device == 3 && type ? accent[1] : accent[device]), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(c, chart_series[n], history[device][type]);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c, LV_OBJ_FLAG_GESTURE_BUBBLE);
    label(parent, x + 22, y + h - 19, 65, "-2m", &lv_font_montserrat_14, MUTED);
    lv_obj_t *now = label(parent, x + w - 60, y + h - 19, 60, "now", &lv_font_montserrat_14, MUTED);
    lv_obj_set_style_text_align(now, LV_TEXT_ALIGN_RIGHT, 0);
}

static void overview(void)
{
    for (int i = 0; i < 2; i++) {
        lv_obj_t *p = card(14 + i * 391, 14, 377, 324);
        clickable(p, i);
        label(p, 18, 14, 70, i ? "GPU" : "CPU", &lv_font_montserrat_20, accent[i]);
        label(p, 250, 18, 110, "DETAILS >", &lv_font_montserrat_14, MUTED);
        names[i] = label(p, 18, 46, 340, "Waiting for sensors", &lv_font_montserrat_16, MUTED);
        temperatures[i] = label(p, 18, 96, 235, "N/A", &lv_font_montserrat_48, TEXT);
        loads[i] = label(p, 252, 101, 104, "N/A", &lv_font_montserrat_32, TEXT);
        lv_obj_set_style_text_align(loads[i], LV_TEXT_ALIGN_RIGHT, 0);
        label(p, 250, 144, 110, "UTILIZATION", &lv_font_montserrat_14, MUTED);
        bars[i] = bar(p, 18, 190, 341, 14, i);
        label(p, 18, 243, 165, "PACKAGE POWER", &lv_font_montserrat_14, MUTED);
        label(p, 207, 243, 155, i ? "HOT SPOT" : "AVERAGE CLOCK", &lv_font_montserrat_14, MUTED);
        powers[i] = label(p, 18, 269, 175, "N/A", &lv_font_montserrat_24, TEXT);
        secondary[i] = label(p, 200, 269, 160, "N/A", &lv_font_montserrat_24, TEXT);
        lv_obj_set_style_text_align(secondary[i], LV_TEXT_ALIGN_RIGHT, 0);
    }
    lv_obj_t *ram = card(14, 348, 377, 112);
    clickable(ram, 2);
    label(ram, 16, 10, 90, "RAM", &lv_font_montserrat_16, TEXT);
    ram_load = label(ram, 270, 10, 90, "N/A", &lv_font_montserrat_16, accent[2]);
    lv_obj_set_style_text_align(ram_load, LV_TEXT_ALIGN_RIGHT, 0);
    ram_value = label(ram, 16, 37, 345, "N/A", &lv_font_montserrat_24, TEXT);
    ram_temps = label(ram, 158, 12, 108, "", &lv_font_montserrat_14, MUTED);
    bars[2] = bar(ram, 16, 84, 345, 12, 2);
    lv_obj_t *net = card(405, 348, 377, 112);
    lv_obj_add_event_cb(net, change_home, LV_EVENT_CLICKED, (void *)(intptr_t)NETWORK);
    label(net, 16, 10, 150, "NETWORK", &lv_font_montserrat_16, accent[3]);
    ping_value = label(net, 210, 10, 150, "N/A", &lv_font_montserrat_14, MUTED);
    lv_obj_set_style_text_align(ping_value, LV_TEXT_ALIGN_RIGHT, 0);
    label(net, 16, 38, 160, "UPLOAD", &lv_font_montserrat_14, MUTED);
    label(net, 200, 38, 160, "DOWNLOAD", &lv_font_montserrat_14, MUTED);
    upload_value = label(net, 16, 65, 172, "N/A", &lv_font_montserrat_24, TEXT);
    download_value = label(net, 200, 65, 165, "N/A", &lv_font_montserrat_24, TEXT);

}

static void charts(void)
{
    for (int i = 0; i < 2; i++) {
        lv_obj_t *p = card(14, 14 + i * 228, 772, 218);
        clickable(p, i);
        label(p, 14, 75, 70, i ? "GPU" : "CPU", &lv_font_montserrat_24, accent[i]);
        label(p, 14, 108, 75, "DETAILS >", &lv_font_montserrat_14, MUTED);
        make_chart(p, 99, 14, 312, 190, i, 0);
        make_chart(p, 435, 14, 312, 190, i, 1);
    }
}

static void network_charts(void)
{
    for (int i = 0; i < 3; i++) {
        lv_obj_t *p = card(14, 14 + i * 152, 772, 142);
        make_chart(p, 14, 10, 742, 122, i < 2 ? 3 : 4, i == 1 ? 1 : 0);
    }
}

static lv_obj_t *stat(int x, const char *caption)
{
    lv_obj_t *p = card(x, 68, 185, 72);
    label(p, 10, 8, 165, caption, &lv_font_montserrat_14, MUTED);
    return label(p, 10, 33, 165, "N/A", &lv_font_montserrat_24, TEXT);
}

static void details(void)
{
    button(content, 14, 14, 88, 44, "< Back", back, 0);
    label(content, 120, 12, 640, detail_device == 0 ? "CPU details" : detail_device == 1 ? "GPU details" : "RAM details",
          &lv_font_montserrat_20, TEXT);
    label(content, 120, 38, 640, detail_device == 0 ? metrics.cpu_name : detail_device == 1 ? metrics.gpu_name : "System memory",
          &lv_font_montserrat_14, MUTED);
    const char *caps[3][4] = {{"UTILIZATION", "TEMPERATURE", "PACKAGE POWER", "AVERAGE CLOCK"},
                             {"UTILIZATION", "TEMPERATURE", "HOT SPOT", "PACKAGE POWER"},
                             {"UTILIZATION", "USED", "TOTAL", "AVAILABLE"}};
    for (int i = 0; i < 4; i++) stat_values[i] = stat(14 + 195 * i, caps[detail_device][i]);
    lv_obj_t *left = card(14, 150, 381, 166), *right = card(405, 150, 381, 166);
    make_chart(left, 12, 10, 355, 144, detail_device, 0);
    make_chart(right, 12, 10, 355, 144, detail_device, 1);
    if (detail_device == 0 && metrics.cores) {
        detail_cores = metrics.cores;
        for (int i = 0; i < detail_cores; i++) {
            int col = i % 8, row = i / 8;
            int height = detail_cores > 16 ? 32 : 60;
            lv_obj_t *p = card(14 + col * 97, 326 + row * height, 90, height - 4);
            char caption[12]; snprintf(caption, sizeof(caption), "T%d", i + 1);
            label(p, 5, 2, 36, caption, &lv_font_montserrat_14, MUTED);
            core_values[i] = label(p, height < 40 ? 35 : 5, height < 40 ? 2 : 20, 52, "N/A", &lv_font_montserrat_14, TEXT);
            core_bars[i] = bar(p, 5, height - 15, 80, 6, 0);
        }
    } else {
        const char *caps_extra[3][4] = {{"LOGICAL THREADS", "CCD TEMPERATURE", "CORE VOLTAGE", "CPU POWER"},
            {"CORE CLOCK", "MEMORY CLOCK", "FAN", "CORE VOLTAGE"},
            {"DIMM #0", "DIMM #1", "DIMM #2", "DIMM #3"}};
        for (int i = 0; i < 4; i++) {
            lv_obj_t *p = card(14 + i * 195, 326, 185, 120);
            label(p, 10, 12, 165, caps_extra[detail_device][i], &lv_font_montserrat_14, MUTED);
            extra_values[i] = label(p, 10, 50, 165, "N/A", &lv_font_montserrat_20, TEXT);
        }
    }
}

static void build_view(void)
{
    lv_obj_clean(content);
    memset(names, 0, sizeof(names)); memset(temperatures, 0, sizeof(temperatures));
    memset(bars, 0, sizeof(bars)); memset(stat_values, 0, sizeof(stat_values));
    memset(extra_values, 0, sizeof(extra_values)); memset(core_values, 0, sizeof(core_values));
    memset(chart_objs, 0, sizeof(chart_objs));
    chart_count = 0; detail_cores = 0;
    if (view == OVERVIEW) overview();
    else if (view == CHARTS) charts();
    else if (view == NETWORK) network_charts();
    else details();
    for (int i = 0; i < 3; i++) {
        if (view == DETAIL) lv_obj_add_flag(pager[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(pager[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(lv_obj_get_child(pager[i], 0), lv_color_hex(home == i ? accent[0] : MUTED), 0);
    }
    update_values();
}

static float ram_temperature(void)
{
    float result = NAN;
    for (int i = 0; i < metrics.dimms; i++) if (isfinite(metrics.dimm_temp[i]))
        result = isfinite(result) ? fmaxf(result, metrics.dimm_temp[i]) : metrics.dimm_temp[i];
    return result;
}

static void update_values(void)
{
    if (view == OVERVIEW) {
        lv_label_set_text(names[0], metrics.cpu_name); lv_label_set_text(names[1], metrics.gpu_name);
        value(temperatures[0], metrics.cpu_temp, " C", 1); value(temperatures[1], metrics.gpu_temp, " C", 1);
        value(loads[0], metrics.cpu_load, "%", 0); value(loads[1], metrics.gpu_load, "%", 0);
        value(powers[0], metrics.cpu_power, " W", 1); value(powers[1], metrics.gpu_power, " W", 1);
        value(secondary[0], metrics.cpu_clock / 1000, " GHz", 2); value(secondary[1], metrics.gpu_hot, " C", 1);
        set_bar(bars[0], metrics.cpu_load); set_bar(bars[1], metrics.gpu_load);
        set_bar(bars[2], metrics.ram_load); value(ram_load, metrics.ram_load, "%", 0);
        pair(ram_value, metrics.ram_used, metrics.ram_total);
        if (metrics.dimms >= 2 && isfinite(metrics.dimm_temp[0]) && isfinite(metrics.dimm_temp[1]))
            lv_label_set_text_fmt(ram_temps, "%.0f / %.0f C", (double)metrics.dimm_temp[0], (double)metrics.dimm_temp[1]);
        else value(ram_temps, ram_temperature(), " C", 0);
        value(upload_value, metrics.upload_mbps, " Mbps", metrics.upload_mbps < 1 ? 3 : 1);
        value(download_value, metrics.download_mbps, " Mbps", metrics.download_mbps < 1 ? 3 : 1);
        value(ping_value, metrics.ping_ms, " ms / Google", 0);
    } else if (view == DETAIL) {
        if (detail_device == 0) {
            value(stat_values[0], metrics.cpu_load, "%", 1); value(stat_values[1], metrics.cpu_temp, " C", 1);
            value(stat_values[2], metrics.cpu_power, " W", 1); value(stat_values[3], metrics.cpu_clock / 1000, " GHz", 2);
            for (int i = 0; i < detail_cores; i++) {
                float load = i < metrics.cores ? metrics.core_load[i] : NAN;
                value(core_values[i], load, "%", 0); set_bar(core_bars[i], load);
            }
            value(extra_values[0], metrics.logical_cores ? metrics.logical_cores : NAN, "", 0);
            value(extra_values[1], metrics.cpu_ccd, " C", 1); value(extra_values[2], metrics.cpu_voltage, " V", 3);
            value(extra_values[3], metrics.cpu_power, " W", 1);
        } else if (detail_device == 1) {
            value(stat_values[0], metrics.gpu_load, "%", 1); value(stat_values[1], metrics.gpu_temp, " C", 1);
            value(stat_values[2], metrics.gpu_hot, " C", 1); value(stat_values[3], metrics.gpu_power, " W", 1);
            value(extra_values[0], metrics.gpu_clock, " MHz", 0); value(extra_values[1], metrics.gpu_memory_clock, " MHz", 0);
            value(extra_values[2], metrics.gpu_fan, " RPM", 0);
            value(extra_values[3], metrics.gpu_voltage, " V", 3);
        } else {
            value(stat_values[0], metrics.ram_load, "%", 1); value(stat_values[1], metrics.ram_used / GIB, " GiB", 1);
            value(stat_values[2], metrics.ram_total / GIB, " GiB", 1); value(stat_values[3], metrics.ram_available / GIB, " GiB", 1);
            for (int i = 0; i < 4; i++) value(extra_values[i], i < metrics.dimms ? metrics.dimm_temp[i] : NAN, " C", 1);
        }
    }
    float current[5][2] = {{metrics.cpu_load, metrics.cpu_temp}, {metrics.gpu_load, metrics.gpu_temp}, {metrics.ram_load, ram_temperature()}, {metrics.upload_mbps, metrics.download_mbps}, {metrics.ping_ms, NAN}};
    for (int i = 0; i < chart_count; i++) {
        int d = chart_devices[i], t = chart_types[i], max = d == 3 ? 1 : 100;
        if (t || d >= 3) for (int j = 0; j < HISTORY; j++) if (history[d][t][j] != LV_CHART_POINT_NONE) {
            int sample = history[d][t][j];
            if (d == 3) { while (max < sample && max < 30000) max = max > 15000 ? 30000 : max * 2; }
            else if (sample > max) max = ((sample / 20) + 1) * 20;
        }
        lv_chart_set_range(chart_objs[i], LV_CHART_AXIS_PRIMARY_Y, 0, max);
        value(chart_values[i], current[d][t], d == 3 ? " Mbps" : d == 4 ? " ms" : t ? " C" : "%", d == 3 && current[d][t] < 1 ? 3 : 1);
        lv_chart_refresh(chart_objs[i]);
    }
}

static void show_settings(void) { wifi_ui_open(root, &prefs); }

static void settings_clicked(lv_event_t *e) { show_settings(); }

static void gesture(lv_event_t *e)
{
    if (wifi_ui_is_open()) return;
    lv_indev_t *input = lv_indev_get_act();
    if (!input) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(input);
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) return;
    lv_indev_wait_release(input);
    if (view == DETAIL) view = home;
    else view = home = (view_t)(((int)home + (dir == LV_DIR_LEFT ? 1 : 2)) % 3);
    build_view();
}

static void tick(lv_timer_t *timer)
{
    int64_t now = esp_timer_get_time();
    metrics_t next;
    if (xQueueReceive(queue, &next, 0) == pdTRUE) {
        last_http = now;
        bool changed = !has_sample || strcmp(next.timestamp, metrics.timestamp);
        metrics = next;
        has_sample = true;
        if (changed && !metrics.stale) { last_change = now; sample_pending = true; }
        if (view == DETAIL && detail_device == 0 && metrics.cores != detail_cores) build_view();
        update_values();
    }
    bool disconnected = !network_connected() || !has_sample || now - last_http > 5000000;
    bool stale = !has_sample || metrics.stale || now - last_change > 5000000;
    uint32_t color = disconnected ? 0xff9b9b : stale || metrics.limited ? 0xffc877 : accent[0];
    lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
    lv_obj_set_style_opa(content, disconnected || stale ? LV_OPA_50 : LV_OPA_COVER, 0);
    if (now - last_history >= 1000000) {
        int slots = last_history ? (int)((now - last_history) / 1000000) : 1;
        if (slots > HISTORY) slots = HISTORY;
        float values[5][2] = {{metrics.cpu_load, metrics.cpu_temp}, {metrics.gpu_load, metrics.gpu_temp}, {metrics.ram_load, ram_temperature()}, {metrics.upload_mbps, metrics.download_mbps}, {metrics.ping_ms, NAN}};
        for (int d = 0; d < 5; d++) for (int t = 0; t < 2; t++) {
            memmove(history[d][t], history[d][t] + slots, (HISTORY - slots) * sizeof(lv_coord_t));
            for (int k = HISTORY - slots; k < HISTORY; k++) history[d][t][k] = LV_CHART_POINT_NONE;
            float v = values[d][t];
            if (sample_pending && !stale && !disconnected && isfinite(v) && v >= 0 && v < 30000)
                history[d][t][HISTORY - 1] = (lv_coord_t)lroundf(fminf(30000, d == 3 ? v * 10 : v));
        }
        last_history = now; sample_pending = false;
        update_values();
    }

}

void ui_start(QueueHandle_t q, const settings_t *settings)
{
    queue = q; prefs = *settings; home = view = (view_t)prefs.home_view;
    metrics_empty(&metrics);
    for (int d = 0; d < 5; d++) for (int t = 0; t < 2; t++) for (int i = 0; i < HISTORY; i++) history[d][t][i] = LV_CHART_POINT_NONE;
    bsp_display_lock(0);
    root = lv_scr_act();
    lv_obj_set_style_bg_color(root, lv_color_hex(BG), 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(root, gesture, LV_EVENT_GESTURE, NULL);
    content = box(root, 0, 0, 800, 480, BG);
    button(root, 758, 4, 36, 36, LV_SYMBOL_SETTINGS, settings_clicked, 0);
    dot = box(root, 749, 5, 6, 6, 0xffc877);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 3; i++) {
        pager[i] = button(root, 334 + i * 44, 460, 44, 20, "", change_home, i);
        lv_obj_set_style_bg_opa(pager[i], LV_OPA_TRANSP, 0);
        lv_obj_del(lv_obj_get_child(pager[i], 0));
        lv_obj_t *indicator = box(pager[i], 13, 7, 18, 6, accent[0]);
        lv_obj_set_style_radius(indicator, 3, 0);
        lv_obj_clear_flag(indicator, LV_OBJ_FLAG_CLICKABLE);
    }
    dimmer = box(root, 0, 0, 800, 480, 0x000000);
    lv_obj_set_style_bg_opa(dimmer, (100 - prefs.brightness) * 255 / 100, 0);
    lv_obj_clear_flag(dimmer, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    build_view();
    lv_timer_create(tick, 100, NULL);
    bsp_display_unlock();
}
