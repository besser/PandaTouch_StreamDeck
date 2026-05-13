#include "streamdeck.h"
#include "storage.h"
#include "ui_main.h"
#include "ble_actions.h"
#include "webserver.h"
#include "l10n.h"
#include "pt/pt_display.h"
#include <ArduinoOTA.h>

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

static void check_sleep_status() {
    // Maps g_sleep_timeout index → inactivity threshold in milliseconds.
    // Index 0 means disabled; indices 1-5 correspond to 30s, 1m, 2m, 5m, 10m.
    static const uint32_t timeout_ms_map[] = {0, 30000, 60000, 120000, 300000, 600000};
    static bool is_sleeping = false;

    if (g_sleep_timeout == 0 || g_sleep_timeout >= 6) {
        if (is_sleeping) {
            pt_set_backlight(g_brightness, false);
            is_sleeping = false;
        }
        return;
    }

    uint32_t inactive_ms = lv_display_get_inactive_time(NULL);
    uint32_t threshold   = timeout_ms_map[g_sleep_timeout];

    if (!is_sleeping && inactive_ms >= threshold) {
        pt_set_backlight(0, false);
        is_sleeping = true;
    } else if (is_sleeping && inactive_ms < threshold) {
        // LVGL reset its timer → a touch woke the device
        pt_set_backlight(g_brightness, false);
        is_sleeping = false;
    }
}

void StreamDeckApp::loop() {
    check_ble_status();
    check_wifi_status();
    check_sleep_status();

    if (g_pending_ui_update) {
        g_pending_ui_update = false;
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
