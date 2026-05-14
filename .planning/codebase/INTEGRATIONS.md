# External Integrations

**Analysis Date:** 2026-05-13

## BLE HID (ESP32 BLE Keyboard)

**Library:** `T-vK/ESP32 BLE Keyboard@^0.3.2`
**Files:** `src/ble_actions.cpp`, `src/ble_actions.h`

**Initialization** (called from `StreamDeckApp::setup()` in `src/streamdeck.cpp`):
```cpp
BleKeyboard bleKeyboard("PandaTouch Deck", "BigTreeTech", 100);

void init_ble() {
    bleKeyboard.begin();
    BLESecurity* pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);   // no MITM, avoids reconnect loop after OTA
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
}
```

**Runtime usage:**
- `bleKeyboard.isConnected()` — checked before every key action
- `bleKeyboard.write(c)` — single character
- `bleKeyboard.press(key)` / `bleKeyboard.releaseAll()` — modifier combos
- Media keys: `KEY_MEDIA_MUTE`, `KEY_MEDIA_VOLUME_UP`, `KEY_MEDIA_PLAY_PAUSE`, etc.
- `check_ble_status()` called every loop iteration — detects rapid disconnect cycles (< 5 s) and auto-clears stale BLE bonds via `esp_ble_remove_bond_device()` after 3 cycles

**Key resolution:**
- `BTN_TYPE_APP` — sends Win+R / Cmd+Space / Alt+F2 then types the app name
- `BTN_TYPE_MEDIA` — maps `"mute"`, `"volup"`, `"play"`, etc. to media keys
- `BTN_TYPE_BASIC_COMBO` — Ctrl/Cmd + single char
- `BTN_TYPE_ADV_COMBO` — parses `"CTRL+SHIFT+S"` style strings via `execute_adv_shortcut()`
- Spanish (`LANG_ES`) keyboard remapping: `ble_write()` translates characters that differ from US layout

**ESP-IDF BLE APIs used directly:**
- `esp_ble_get_bond_device_num()`, `esp_ble_get_bond_device_list()`, `esp_ble_remove_bond_device()` — bond management
- `esp_gap_ble_api.h` included directly

---

## WiFi

**Library:** Arduino built-in `WiFi`
**Files:** `src/storage.cpp`, `src/webserver.cpp`, `src/streamdeck.cpp`

**Initialization:**
```cpp
// In StreamDeckApp::setup():
WiFi.mode(WIFI_STA);

// In load_settings() (src/storage.cpp), after reading NVS:
if (strlen(g_wifi_ssid) > 0 && WiFi.status() != WL_CONNECTED) {
    WiFi.begin(g_wifi_ssid, g_wifi_pass);
}
```

**Runtime monitoring (`check_wifi_status()` in `src/webserver.cpp`):**
- Polled every 2000 ms in `StreamDeckApp::loop()`
- On connect: calls `init_webserver()`, updates `g_wifi_status` / `g_ip_addr` globals, refreshes UI label
- On disconnect: clears status strings

**Credentials storage:** NVS keys `"wssid"` and `"wpass"` in namespace `"deck"` (plain text in NVS — not encrypted)

---

## ESPAsyncWebServer

**Library:** GitHub `me-no-dev/ESPAsyncWebServer` + `me-no-dev/AsyncTCP`
**Files:** `src/webserver.cpp`, `src/webserver.h`, `src/webserver_html.h`

**Initialization:** `init_webserver()` — called once when WiFi connects. Guard: `webserver_started` flag prevents double-registration.

**Server instance:** `AsyncWebServer server(80)` — HTTP on port 80, no HTTPS.

**API Endpoints:**

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | Serve web dashboard HTML (inlined in `FPSTR(INDEX_HTML)`, version substituted) |
| GET | `/api/config` | Return full config + all button configs as JSON |
| GET | `/api/l10n` | Return localization strings as JSON |
| POST | `/api/save` | Save settings + button configs from form params; triggers `g_pending_ui_update` |
| GET | `/api/backup` | Export full config + all OS button files + assets as JSON (assets base64-encoded) |
| POST | `/api/restore` | Import JSON backup; body accumulated in `request->_tempObject` (max 512 KB) |
| GET | `/api/files` | List LittleFS files (excludes `*.bin` system files) |
| POST | `/api/delete` | Delete a LittleFS file (system `.bin` files are forbidden) |
| POST | `/api/upload` | Upload image/asset file to LittleFS via multipart |
| POST | `/api/update` | OTA firmware update via multipart binary upload |

**JSON library:** `bblanchon/ArduinoJson@^7.0.0` — `JsonDocument`, `serializeJson()`, `deserializeJson()`

**HTML delivery:** The dashboard HTML is stored as a PROGMEM string in `src/webserver_html.h` via `FPSTR()`. Version placeholder `__VERSION__` is replaced at request time.

---

## ArduinoOTA

**Library:** Arduino built-in `ArduinoOTA`
**Files:** `src/streamdeck.cpp`, `src/webserver.cpp`

**Two OTA paths exist:**

1. **ArduinoOTA (mDNS/UDP)** — Traditional Arduino IDE / PlatformIO OTA over UDP:
   ```cpp
   // Registered in StreamDeckApp::setup():
   ArduinoOTA.onStart([]() { pt_enter_ota_mode(); ... });
   ArduinoOTA.begin();
   // Polled in StreamDeckApp::loop():
   ArduinoOTA.handle();
   ```
   Callbacks: `onStart`, `onEnd`, `onProgress`, `onError`

2. **Web OTA (`/api/update`)** — Binary upload via HTTP multipart in `src/webserver.cpp`:
   - Uses ESP-IDF `Update` class directly (`Update.begin()`, `Update.write()`, `Update.end()`)
   - Max size enforced: 4 MB
   - Progress tracked via global `g_ota_progress` int
   - On OTA start: sets `g_ota_screen_requested = true` flag → main loop calls `pt_enter_ota_mode()` which frees LVGL buffers, suspends LVGL, draws "Updating..." text directly via GFX
   - On success: `ESP.restart()` after 1.5 s delay

---

## LVGL Touch Driver (TAMC_GT911)

**Library:** `tamctec/TAMC_GT911@1.0.2`
**Files:** `src/pt/pt_display.h`, `src/pt/pt_board.h`

**Hardware connection:**
- I2C0: SCL=GPIO1, SDA=GPIO2, speed 100 kHz
- IRQ: GPIO40, RST: GPIO41

**Initialization** (inside `pt_setup_display()`):
```cpp
pt_touchpanel.begin();
pt_touchpanel.setRotation(1);
```

**LVGL input device registration:**
```cpp
lv_indev_t *indev = lv_indev_create();
lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
lv_indev_set_read_cb(indev, pt_touchpad_read);
```

**Read callback `pt_touchpad_read()`:**
- Calls `pt_touchpanel.read()` each LVGL tick
- Validates coordinates (rejects > 800 or > 480 to guard against I2C glitches returning 65535)
- Updates `pt_last_touch_ms` (used by sleep inactivity timer)
- Only primary touch point (index 0) is forwarded to LVGL

---

## LVGL Display Driver (Arduino GFX Library)

**Library:** `moononournation/GFX Library for Arduino@1.5.0`
**Files:** `src/pt/pt_display.h`, `src/pt/pt_board.h`

**Panel type:** `Arduino_ESP32RGBPanel` — drives 800x480 RGB-DE panel
**Display object:** `Arduino_RGB_Display pt_gfx`

**LVGL flush callback `pt_disp_flush()`:**
```cpp
pt_gfx.draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
lv_disp_flush_ready(disp);
```

**Render buffers** (allocated in `pt_setup_display()`):
- Default mode: `PT_LVGL_RENDER_PARTIAL_2_PSRAM` — two partial buffers (800 × 80 lines each) in PSRAM
- Fallback chain: PSRAM → internal heap → single buffer if allocation fails
- Buffers freed in `pt_enter_ota_mode()` to reclaim RAM before firmware write

**Backlight:**
- GPIO21 via LEDC (channel 0, timer 1, 11-bit resolution, 30 kHz)
- `pt_set_backlight(percent, save)` — converts 0–100% to duty cycle
- Brightness persisted in NVS key `"bright"` (default 50%)

---

## LittleFS

**Library:** Arduino built-in `LittleFS`
**Files:** `src/storage.cpp`, `src/webserver.cpp`

**Initialization:**
```cpp
// In init_storage():
LittleFS.begin(true);  // format on failure
```

**Files stored:**
| Path | Contents |
|------|----------|
| `/win_btns_v2.bin` | Windows button configs — raw `ButtonConfig[100]` struct array |
| `/mac_btns_v2.bin` | macOS button configs — same format |
| `/linux_btns_v2.bin` | Linux button configs — same format |
| `/<name>.png` / `/<name>.jpg` | User-uploaded button icon images |

**Migration:** On first boot with v2 firmware, `load_settings()` auto-migrates v1 single-page files (`/win_btns.bin`) to v2 multi-page format (`/win_btns_v2.bin`) by padding with disabled buttons.

**LVGL filesystem bridge:** `LV_USE_FS_ARDUINO_ESP_LITTLEFS = 1`, driver letter `'L'` — allows LVGL image widgets to load images from LittleFS using paths like `"L:/icon.png"`.

---

## NVS Preferences

**Library:** Arduino built-in `Preferences` (wraps ESP-IDF NVS)
**Files:** `src/storage.cpp`

**Namespace:** `"deck"` (opened read-write)

**Keys stored:**
| Key | Type | Default | Meaning |
|-----|------|---------|---------|
| `"rows"` | UChar | 3 | Grid rows (1–5) |
| `"cols"` | UChar | 3 | Grid columns (1–5) |
| `"os"` | UChar | 0 | Target OS (0=Windows, 1=macOS, 2=Linux) |
| `"lang"` | UChar | 0 | Keyboard language (0=US, 1=ES) |
| `"bg"` | UInt | 0x121212 | Background color (RGB hex) |
| `"bright"` | UChar | 50 | Backlight brightness % |
| `"num_pages"` | UChar | 5 | Number of pages |
| `"sleep_to"` | UChar | 0 | Sleep timeout index (0=off, 1=30s, …5=10min) |
| `"page"` | UChar | 0 | Current page index |
| `"wssid"` | String | "" | WiFi SSID |
| `"wpass"` | String | "" | WiFi password |

**Usage pattern:**
```cpp
preferences.begin("deck", false);
g_rows = preferences.getUChar("rows", 3);
// ... read all keys ...
preferences.end();
```
`preferences.begin()` / `preferences.end()` called as a pair in both `load_settings()` and `save_settings()`.

---

## Monitoring & Observability

**Error Tracking:** None (no Sentry, no crash reporter)

**Logs:** `Serial.println()` / `Serial.printf()` at 115200 baud — used throughout for BLE events, WiFi, OTA progress, storage operations. LVGL logging disabled (`LV_USE_LOG 0`).

**Watchdog:** `esp_task_wdt_reset()` called explicitly during OTA write chunks to prevent WDT reset during long flash operations.

---

## CI/CD & Deployment

**Hosting:** No cloud hosting; firmware runs on device.

**CI Pipeline:** None detected.

**Deployment paths:**
1. PlatformIO upload over USB (initial flash)
2. ArduinoOTA over WiFi (development)
3. Web dashboard `/api/update` endpoint (end-user firmware updates)

---

*Integration audit: 2026-05-13*
