#include "streamdeck.h"
#include "storage.h"
#include "ui_main.h"
#include "ble_actions.h"
#include "webserver.h"
#include "l10n.h"
#include "pt/pt_display.h"
#include <ArduinoOTA.h>

// Transparent LVGL overlay placed above all widgets when the display sleeps.
// The first touch lands here (not on any button), wakes the display, and
// destroys the overlay — so subsequent touches reach widgets normally.
static lv_obj_t* g_sleep_overlay = nullptr;

static void sleep_overlay_cb(lv_event_t* e) {
    (void)e;
    lv_obj_t* overlay = g_sleep_overlay;
    g_sleep_overlay    = nullptr;
    pt_set_backlight(g_brightness, false);
    pt_last_touch_ms = millis();
    lv_obj_del_async(overlay);
}

static void enter_sleep() {
    pt_set_backlight(0, false);

    g_sleep_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_sleep_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(g_sleep_overlay, 0, 0);
    lv_obj_set_style_bg_opa(g_sleep_overlay,    LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_sleep_overlay, 0,          LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_sleep_overlay,     0,           LV_PART_MAIN);
    lv_obj_add_flag(g_sleep_overlay,   LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_sleep_overlay,   LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_clear_flag(g_sleep_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_sleep_overlay, sleep_overlay_cb, LV_EVENT_PRESSED, nullptr);
}

static void check_sleep_status() {
    static const uint32_t timeout_ms_map[] = {0, 30000, 60000, 120000, 300000, 600000};

    if (pt_last_touch_ms == 0) pt_last_touch_ms = millis();

    // Sleeping: overlay handles wake, nothing to do here.
    if (g_sleep_overlay != nullptr) return;

    if (g_sleep_timeout == 0 || g_sleep_timeout >= 6) return;

    uint32_t threshold = timeout_ms_map[g_sleep_timeout];
    if ((millis() - pt_last_touch_ms) >= threshold) {
        enter_sleep();
    }
}

void StreamDeckApp::setup() {
    WiFi.mode(WIFI_STA);

    init_storage();
    load_settings();

    g_main_screen = lv_scr_act();
    create_main_ui();

    Serial.println("StreamDeckApp::setup() - Starting BLE initialization");
    delay(500);

    init_ble();

    ArduinoOTA.onStart([]() {
        pt_enter_ota_mode();
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("OTA: Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA: Update Complete");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        (void)progress; (void)total;
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
}

void StreamDeckApp::loop() {
    check_ble_status();
    check_wifi_status();
    check_sleep_status();

    if (g_pending_ui_update) {
        g_pending_ui_update = false;
        // If sleeping, the overlay lives on the current screen and will be
        // destroyed by lv_obj_clean inside create_main_ui. Wake the display
        // so the user sees the refreshed content.
        if (g_sleep_overlay) {
            g_sleep_overlay = nullptr;
            pt_set_backlight(g_brightness, false);
            pt_last_touch_ms = millis();
        }
        lv_scr_load(g_main_screen);
        create_main_ui();
    }

    if (g_ota_screen_requested) {
        g_ota_screen_requested = false;
        pt_enter_ota_mode();
    }

    ArduinoOTA.handle();
    yield();
}

void StreamDeckApp::handle_button(uint8_t idx) {
    handle_button_action(idx);
}

void StreamDeckApp::log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
