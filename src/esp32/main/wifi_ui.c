#include "wifi_ui.h"
#include "network.h"
#include <stdio.h>
#include <string.h>
#include "esp_system.h"

#define BG 0x101417
#define PANEL 0x1b2227
#define TEXT 0xf3f6f8
#define MUTED 0x9fadb5
static lv_obj_t *panel, *form, *nearby, *list, *keyboard, *footer, *message;
static lv_obj_t *ssid_input, *password_input, *url_input, *open_network, *interval, *brightness, *home_view;
static lv_timer_t *scan_timer;
static lv_obj_t *active_input;
static settings_t saved;
static wifi_scan_t results;
static bool showing_scan;

bool wifi_ui_is_open(void) { return panel != NULL; }

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, const char *text)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, w);
    lv_obj_set_style_text_font(o, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(TEXT), 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_DOT);
    lv_label_set_text(o, text);
    return o;
}

static lv_obj_t *container(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(BG), 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    return o;
}

static lv_obj_t *button(lv_obj_t *parent, int x, int y, int w, int h, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x303c43), 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = label(b, 0, 0, w - 12, text);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    return b;
}

static void hide_keyboard(void)
{
    lv_obj_t *input = lv_keyboard_get_textarea(keyboard);
    if (input) lv_obj_clear_state(input, LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(keyboard, NULL);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(form, 330); lv_obj_set_height(nearby, 330);
    lv_obj_set_height(list, 276);
}

static void keyboard_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) hide_keyboard();
}

static void show_keyboard(lv_obj_t *input)
{
    if (!keyboard || !input) return;
    lv_obj_t *previous = lv_keyboard_get_textarea(keyboard);
    if (previous && previous != input) lv_obj_clear_state(previous, LV_STATE_FOCUSED);
    active_input = input;
    lv_obj_add_state(input, LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(keyboard, input);
    lv_obj_set_size(keyboard, 800, 210);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_move_foreground(keyboard);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(form, 192); lv_obj_set_height(nearby, 192);
    lv_obj_set_height(list, 138);
    lv_obj_update_layout(panel);
    lv_obj_scroll_to_view_recursive(input, LV_ANIM_ON);
}

static void focus_input(lv_event_t *e) { show_keyboard(lv_event_get_target(e)); }
static void keyboard_clicked(lv_event_t *e) { show_keyboard(active_input ? active_input : ssid_input); }

static lv_obj_t *input(int y, const char *caption, const char *text, int max)
{
    label(form, 4, y, 424, caption);
    lv_obj_t *o = lv_textarea_create(form);
    lv_obj_set_pos(o, 4, y + 24); lv_obj_set_size(o, 428, 44);
    lv_textarea_set_one_line(o, true); lv_textarea_set_max_length(o, max);
    lv_obj_set_style_text_font(o, &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(PANEL), 0);
    lv_obj_set_style_text_color(o, lv_color_hex(TEXT), 0);
    lv_textarea_set_text(o, text);
    lv_obj_add_event_cb(o, focus_input, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(o, focus_input, LV_EVENT_CLICKED, NULL);
    return o;
}

static void close_settings(lv_event_t *e)
{
    if (scan_timer) lv_timer_del(scan_timer);
    scan_timer = NULL;
    if (panel) lv_obj_del(panel);
    panel = NULL; keyboard = NULL; active_input = NULL;
}

static void reboot(lv_timer_t *timer) { esp_restart(); }

static void save(lv_event_t *e)
{
    settings_t next = saved;
    const char *ssid = lv_textarea_get_text(ssid_input), *pw = lv_textarea_get_text(password_input);
    const char *url = lv_textarea_get_text(url_input);
    if (strlen(ssid) >= sizeof(next.ssid) || strlen(pw) >= sizeof(next.password) || strlen(url) >= sizeof(next.url)) {
        lv_label_set_text(message, "A field is too long. Use a shorter SSID, password or URL."); return;
    }
    if (lv_obj_has_state(open_network, LV_STATE_CHECKED)) next.password[0] = 0;
    else if (pw[0]) snprintf(next.password, sizeof(next.password), "%s", pw);
    else if (strcmp(ssid, saved.ssid) || !saved.password[0]) {
        lv_label_set_text(message, "Enter the Wi-Fi password, or select Open network."); return;
    }
    snprintf(next.ssid, sizeof(next.ssid), "%s", ssid);
    snprintf(next.url, sizeof(next.url), "%s", url);
    static const uint16_t intervals[] = {500, 1000, 2000, 5000, 10000};
    next.interval_ms = intervals[lv_dropdown_get_selected(interval)];
    next.brightness = lv_slider_get_value(brightness);
    next.home_view = lv_dropdown_get_selected(home_view);
    if (!settings_valid(&next)) { lv_label_set_text(message, "Check the SSID, password (8-63 characters), and http:// PC URL."); return; }
    if (settings_save(&next) != ESP_OK) { lv_label_set_text(message, "Could not save settings. Please try again."); return; }
    lv_label_set_text(message, "Saved. Restarting and connecting...");
    lv_obj_add_state(lv_event_get_target(e), LV_STATE_DISABLED);
    lv_timer_t *timer = lv_timer_create(reboot, 800, NULL);
    lv_timer_set_repeat_count(timer, 1);
}

static void select_network(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    if (index < 0 || index >= results.count) return;
    wifi_network_t *n = &results.networks[index];
    lv_textarea_set_text(ssid_input, n->ssid);
    lv_textarea_set_text(password_input, "");
    if (n->open) lv_obj_add_state(open_network, LV_STATE_CHECKED);
    else lv_obj_clear_state(open_network, LV_STATE_CHECKED);
    lv_label_set_text(message, n->open ? "Open network selected. Check the PC URL, then Save and connect." : "Network selected. Enter its password, then Save and connect.");
    lv_event_send(password_input, LV_EVENT_FOCUSED, NULL);
}

static void rescan(lv_event_t *e)
{
    network_request_scan(); showing_scan = true;
    lv_label_set_text(message, "Scanning for nearby Wi-Fi networks...");
}

static void scan_tick(lv_timer_t *timer)
{
    if (!showing_scan || network_scan_busy() || !network_get_scan(&results)) return;
    showing_scan = false;
    lv_obj_clean(list);
    if (results.error != ESP_OK) {
        lv_list_add_text(list, "Scan unavailable. Tap Rescan.");
        lv_label_set_text(message, "Scan not ready. You can enter the network name manually."); return;
    }
    if (!results.count) lv_list_add_text(list, "No networks found. Enter the SSID manually.");
    for (int i = 0; i < results.count; i++) {
        char text[80];
        snprintf(text, sizeof(text), "%s\n%d dBm / %s", results.networks[i].ssid, results.networks[i].rssi,
                 results.networks[i].open ? "Open" : "Secured");
        lv_obj_t *b = lv_list_add_btn(list, LV_SYMBOL_WIFI, text);
        lv_obj_set_style_bg_color(b, lv_color_hex(PANEL), 0);
        lv_obj_set_style_text_color(b, lv_color_hex(TEXT), 0);
        lv_obj_set_style_text_font(b, &lv_font_montserrat_16, 0);
        lv_obj_set_height(b, 64);
        lv_obj_add_event_cb(b, select_network, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_label_set_text(message, network_connected() ? "Connected. Select a network to change it." : "Select a network or enter its name manually.");
}

void wifi_ui_open(lv_obj_t *parent, const settings_t *settings)
{
    if (panel) return;
    saved = *settings;
    panel = container(parent, 0, 0, 800, 480);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    button(panel, 14, 12, 96, 44, "< Back", close_settings);
    label(panel, 128, 12, 510, "Wi-Fi and display settings");
    button(panel, 650, 12, 136, 44, "Keyboard", keyboard_clicked);
    message = label(panel, 128, 38, 510, "Choose a network or enter its name manually.");
    lv_obj_set_style_text_font(message, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(message, lv_color_hex(MUTED), 0);
    form = container(panel, 14, 66, 448, 330);
    lv_obj_set_scroll_dir(form, LV_DIR_VER);
    nearby = container(panel, 480, 66, 306, 330);
    lv_obj_clear_flag(nearby, LV_OBJ_FLAG_SCROLLABLE);
    label(nearby, 0, 12, 190, "Nearby networks");
    button(nearby, 196, 0, 110, 44, "Rescan", rescan);
    list = lv_list_create(nearby);
    lv_obj_set_pos(list, 0, 54); lv_obj_set_size(list, 306, 276);
    lv_obj_set_style_bg_color(list, lv_color_hex(BG), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    ssid_input = input(0, "Wi-Fi network name", saved.ssid, 32);
    password_input = input(82, "Password (blank keeps the saved password)", "", 63);
    lv_textarea_set_password_mode(password_input, true);
    open_network = lv_checkbox_create(form);
    lv_obj_set_pos(open_network, 4, 158); lv_checkbox_set_text(open_network, "Open network (no password)");
    lv_obj_set_style_text_color(open_network, lv_color_hex(TEXT), 0);
    if (saved.ssid[0] && !saved.password[0]) lv_obj_add_state(open_network, LV_STATE_CHECKED);
    url_input = input(204, "PC metrics URL (http://PC-IP:8080/api/metrics)", saved.url, 255);
    label(form, 4, 288, 420, "Update interval");
    interval = lv_dropdown_create(form); lv_obj_set_pos(interval, 4, 312); lv_obj_set_size(interval, 428, 44);
    lv_dropdown_set_options(interval, "0.5 seconds\n1 second\n2 seconds\n5 seconds\n10 seconds");
    static const uint16_t intervals[] = {500, 1000, 2000, 5000, 10000};
    for (int i = 0; i < 5; i++) if (saved.interval_ms == intervals[i]) lv_dropdown_set_selected(interval, i);
    label(form, 4, 376, 420, "Display brightness (software dimming)");
    brightness = lv_slider_create(form); lv_obj_set_pos(brightness, 14, 414); lv_obj_set_size(brightness, 402, 18);
    lv_slider_set_range(brightness, 10, 100); lv_slider_set_value(brightness, saved.brightness, LV_ANIM_OFF);
    label(form, 4, 458, 420, "Start screen");
    home_view = lv_dropdown_create(form); lv_obj_set_pos(home_view, 4, 482); lv_obj_set_size(home_view, 428, 44);
    lv_dropdown_set_options(home_view, "Overview\nCharts\nNetwork"); lv_dropdown_set_selected(home_view, saved.home_view);
    /* Extra bottom space makes the final field easy to scroll into view. */
    label(form, 4, 538, 420, "Settings are saved on this display.");
    footer = container(panel, 14, 410, 772, 56);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    button(footer, 0, 0, 380, 52, "Save and connect", save);
    button(footer, 392, 0, 380, 52, "Cancel", close_settings);
    keyboard = lv_keyboard_create(panel);
    lv_obj_set_size(keyboard, 800, 210);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    active_input = ssid_input;
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(keyboard, keyboard_event, LV_EVENT_ALL, NULL);
    scan_timer = lv_timer_create(scan_tick, 200, NULL);
    showing_scan = true; network_request_scan();
}
