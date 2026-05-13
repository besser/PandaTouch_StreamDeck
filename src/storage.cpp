#include "storage.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <WiFi.h>

struct LegacyButtonConfig {
    char label[16];
    char value[128];
    uint8_t type;
    uint32_t color;
    char icon[8];
    char imgPath[32];
};

ButtonConfig g_configs[MAX_TOTAL_BUTTONS];
uint32_t g_bg_color = 0x121212;
uint8_t g_rows = 3;
uint8_t g_cols = 3;
uint8_t g_target_os = OS_WINDOWS;
uint8_t g_current_page = 0;
char g_wifi_ssid[32] = "";
char g_wifi_pass[64] = "";
uint8_t g_kb_lang = LANG_US;
uint8_t g_brightness = 50;
uint8_t g_num_pages = MAX_PAGES;
uint8_t g_sleep_timeout = 0;
String g_wifi_status = "Disconnected";
String g_ip_addr = "0.0.0.0";

static Preferences preferences;

void init_storage() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    } else {
        Serial.println("LittleFS Mounted Successfully.");
    }
}

void load_settings() {
    preferences.begin("deck", false);
    g_rows = preferences.getUChar("rows", 3);
    g_cols = preferences.getUChar("cols", 3);
    g_target_os = preferences.getUChar("os", 0);
    g_kb_lang = preferences.getUChar("lang", 0);
    g_bg_color = preferences.getUInt("bg", 0x121212);
    g_brightness = preferences.getUChar("bright", 50);
    g_num_pages = preferences.getUChar("num_pages", MAX_PAGES);
    g_sleep_timeout = preferences.getUChar("sleep_to", 0);
    g_current_page = preferences.getUChar("page", 0);

    if (g_bg_color == 0x000000) g_bg_color = 0x121212;
    if (g_brightness < 1) g_brightness = 50;
    if (g_brightness > 100) g_brightness = 100;
    if (g_rows < 1 || g_rows > 5) g_rows = 3;
    if (g_cols < 1 || g_cols > 5) g_cols = 3;
    if (g_num_pages < 1 || g_num_pages > MAX_PAGES) g_num_pages = MAX_PAGES;
    if (g_current_page >= g_num_pages) g_current_page = 0;

    const char* win_file = "/win_btns_v2.bin";
    const char* mac_file = "/mac_btns_v2.bin";
    const char* linux_file = "/linux_btns_v2.bin";

    // Simple migration from v1 (single page) to v2 (multi page)
    auto migrate_v1_to_v2 = [](const char* old_path, const char* new_path) {
        if (LittleFS.exists(new_path)) return;
        if (!LittleFS.exists(old_path)) return;

        Serial.printf("STORAGE: Migrating %s to %s...\n", old_path, new_path);
        File f_old = LittleFS.open(old_path, "r");
        if (!f_old) return;

        ButtonConfig* temp_v2 = (ButtonConfig*)malloc(sizeof(ButtonConfig) * MAX_TOTAL_BUTTONS);
        if (!temp_v2) {
            f_old.close();
            return;
        }
        memset(temp_v2, 0, sizeof(ButtonConfig) * MAX_TOTAL_BUTTONS);
        
        // Default values for all buttons
        for(int i=0; i<MAX_TOTAL_BUTTONS; i++) {
            temp_v2[i].color = 0x333333;
            sprintf(temp_v2[i].label, "Btn %d", i+1);
        }

        // Load page 1 from old file
        f_old.read((uint8_t*)temp_v2, BUTTONS_PER_PAGE * sizeof(ButtonConfig));
        f_old.close();

        File f_new = LittleFS.open(new_path, "w");
        if (f_new) {
            f_new.write((uint8_t*)temp_v2, sizeof(ButtonConfig) * MAX_TOTAL_BUTTONS);
            f_new.close();
            Serial.println("STORAGE: Migration V2 successful.");
        }
        free(temp_v2);
    };

    migrate_v1_to_v2("/win_btns.bin", win_file);
    migrate_v1_to_v2("/mac_btns.bin", mac_file);
    migrate_v1_to_v2("/linux_btns.bin", linux_file);

    const char* active_file;
    if (g_target_os == OS_WINDOWS) active_file = win_file;
    else if (g_target_os == OS_MACOS) active_file = mac_file;
    else active_file = linux_file;

    File f = LittleFS.open(active_file, "r");
    if (f) {
        size_t read = f.read((uint8_t*)g_configs, sizeof(g_configs));
        f.close();
        if (read != sizeof(g_configs)) {
            Serial.println("STORAGE: size mismatch, resetting");
            goto load_defaults;
        }
    } else {
    load_defaults:
        for (int i = 0; i < MAX_TOTAL_BUTTONS; i++) {
            memset(&g_configs[i], 0, sizeof(ButtonConfig));
            g_configs[i].color = 0x333333;
            sprintf(g_configs[i].label, "Btn %d", i+1);
        }
    }

    preferences.getString("wssid", g_wifi_ssid, 31);
    preferences.getString("wpass", g_wifi_pass, 63);
    preferences.end();

    if (strlen(g_wifi_ssid) > 0 && WiFi.status() != WL_CONNECTED) {
        WiFi.begin(g_wifi_ssid, g_wifi_pass);
    }
}

void save_settings(bool saveButtons) {
    preferences.begin("deck", false);
    preferences.putUInt("bg", g_bg_color);
    preferences.putUChar("bright", g_brightness);
    preferences.putUChar("rows", g_rows);
    preferences.putUChar("cols", g_cols);
    preferences.putUChar("os", g_target_os);
    preferences.putUChar("lang", g_kb_lang);
    preferences.putUChar("num_pages", g_num_pages);
    preferences.putUChar("sleep_to", g_sleep_timeout);
    preferences.putUChar("page", g_current_page);
    preferences.putString("wssid", g_wifi_ssid);
    preferences.putString("wpass", g_wifi_pass);
    preferences.end();

    if (saveButtons) {
        const char* active_file;
        if (g_target_os == OS_WINDOWS) active_file = "/win_btns_v2.bin";
        else if (g_target_os == OS_MACOS) active_file = "/mac_btns_v2.bin";
        else active_file = "/linux_btns_v2.bin";

        File f = LittleFS.open(active_file, "w");
        if (f) {
            f.write((uint8_t*)g_configs, sizeof(g_configs));
            f.close();
        }
    }
}
