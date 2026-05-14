# Coding Conventions

**Analysis Date:** 2026-05-13

## Language and Standard

- C++17 (Arduino framework on ESP32-S3)
- All source in `src/` — `.cpp` implementation, `.h` header with `#pragma once` + traditional include guards (both present simultaneously)
- No namespace usage; flat global scope with naming conventions used to avoid collisions

## Naming Patterns

**Global variables — `g_` prefix (mandatory):**
All shared application state uses the `g_` prefix. Defined in `src/storage.cpp`, declared `extern` in `src/storage.h`.
```cpp
extern ButtonConfig g_configs[MAX_TOTAL_BUTTONS];
extern uint32_t g_bg_color;
extern uint8_t g_rows;
extern uint8_t g_cols;
extern uint8_t g_current_page;
extern String g_wifi_status;
extern String g_ip_addr;
```

Module-local statics in UI files also use `g_` when they are screen object pointers:
```cpp
// src/ui_settings.cpp
static lv_obj_t* g_settings_screen = nullptr;
static lv_obj_t* g_edit_screen = nullptr;
static lv_obj_t* g_wifi_screen = nullptr;
```

**LVGL event callbacks — `_cb` suffix (mandatory):**
Every function registered with `lv_obj_add_event_cb()` ends in `_cb`.
```cpp
static void btn_event_cb(lv_event_t* e)       // src/ui_main.cpp
static void slider_event_cb(lv_event_t* e)    // src/ui_main.cpp
static void settings_btn_cb(lv_event_t* e)    // src/ui_main.cpp
static void back_to_main_cb(lv_event_t* e)    // src/ui_settings.cpp
static void save_edit_cb(lv_event_t* e)        // src/ui_settings.cpp
static void color_slider_cb(lv_event_t* e)    // src/ui_settings.cpp
static void kb_focus_cb(lv_event_t* e)        // src/ui_settings.cpp
static void sleep_overlay_cb(lv_event_t* e)   // src/streamdeck.cpp
```
Selection callbacks that are NOT directly registered with LVGL (passed as `selection_cb_t`) drop the `_cb` suffix: `grid_selected`, `os_selected`, `lang_selected`, `pages_selected`, `sleep_selected`.

**Hardware/display functions — `pt_` prefix:**
All functions in `src/pt/pt_display.h` use the `pt_` prefix, matching the hardware module namespace.
```cpp
pt_setup_display()
pt_loop_display()
pt_set_backlight()
pt_enter_ota_mode()
pt_disp_flush()
pt_touchpad_read()
```

**Board constants — `PT_` prefix (all caps):**
Pin and timing constants in `src/pt/pt_board.h`:
```cpp
#define PT_LCD_H_RES 800
#define PT_LCD_BL_PIN 21
#define PT_GT911_IRQ_PIN 40
```

**Application constants — all caps with no prefix:**
```cpp
#define MAX_PAGES 5
#define BUTTONS_PER_PAGE 20
#define PANDA_VERSION "1.8.0"
```

**Functions:**
- `snake_case` for all free functions: `init_storage()`, `load_settings()`, `save_settings()`, `init_ble()`, `handle_button_action()`
- `PascalCase::snake_case` for the single class: `StreamDeckApp::setup()`, `StreamDeckApp::loop()`

**Structs:**
- `PascalCase`: `ButtonConfig`, `SelectionCtx`, `WifiUIData`, `EditUIData`, `LegacyButtonConfig`, `L10n`

**Enums:**
- Type in `PascalCase`: `ButtonType`, `TargetOS`, `KbLang`
- Values in `UPPER_SNAKE_CASE` with type prefix: `BTN_TYPE_APP`, `OS_WINDOWS`, `LANG_US`

**Local variables:**
- `snake_case`
- Short single-purpose vars use abbreviated names: `r`, `g`, `b` for RGB; `e` for event; `l` for L10n pointer; `f` for File

## File Structure

**Header guard pattern** (both `#pragma once` and `#ifndef` guard, redundant but consistent):
```cpp
#pragma once
#ifndef PT_CONSTANTS_H
#define PT_CONSTANTS_H
// ...
#endif
```

**Forward declarations block** before callback implementations in `src/ui_settings.cpp`:
```cpp
// ========== Forward Declarations ==========
static void back_to_main_cb(lv_event_t* e);
static void save_edit_cb(lv_event_t* e);
// ...
```

**Section comment dividers** using `// ========== Section Name ==========` (seen in `src/ui_settings.cpp`).

**Include order** (observed pattern):
1. Own header (e.g., `#include "ui_main.h"`)
2. Sibling module headers
3. Third-party/Arduino headers

## LVGL Object Lifecycle Patterns

**Screen creation:** Always check pointer before creating; delete old screen synchronously before creating new one when NOT inside a callback:
```cpp
// src/ui_settings.cpp:create_settings_ui
if (g_settings_screen == nullptr || g_settings_needs_rebuild) {
    if (g_settings_screen != nullptr) {
        lv_obj_del(g_settings_screen);  // sync delete, not in callback
    }
    g_settings_screen = lv_obj_create(NULL);
    // ...
}
lv_scr_load(g_settings_screen);
```

**Async delete from callbacks (`lv_obj_del_async`):** When a callback must delete the screen it is registered on (deleting self), always use `lv_obj_del_async` to defer until after the event handler returns. Set pointer to `nullptr` immediately after scheduling:
```cpp
// src/ui_settings.cpp:back_to_main_cb
lv_scr_load(g_main_screen);
if (g_edit_screen)        { lv_obj_del_async(g_edit_screen);        g_edit_screen        = nullptr; }
if (g_wifi_screen)        { lv_obj_del_async(g_wifi_screen);        g_wifi_screen        = nullptr; }
if (g_button_list_screen) { lv_obj_del_async(g_button_list_screen); g_button_list_screen = nullptr; }
```

**Sleep overlay:** Uses `lv_obj_del_async` because the touch handler deletes the overlay object it is attached to:
```cpp
// src/streamdeck.cpp:sleep_overlay_cb
lv_obj_t* overlay = g_sleep_overlay;
g_sleep_overlay    = nullptr;  // clear global first
lv_obj_del_async(overlay);    // defer delete
```

**Main screen refresh:** The main screen (`g_main_screen`) is never deleted. Instead, `lv_obj_clean()` clears all children in place, then children are recreated. This preserves the screen handle assigned at startup:
```cpp
// src/ui_main.cpp:create_main_ui
lv_obj_clean(g_main_screen);
lv_obj_set_style_bg_color(g_main_screen, lv_color_hex(g_bg_color), LV_PART_MAIN);
// recreate children...
```

**Null guard before LVGL calls:** Check that grid/label pointers are non-null before operating on them:
```cpp
// src/ui_main.cpp:refresh_main_ui
if (!g_grid) return;
// src/ui_main.cpp:update_ota_progress
if (!g_update_screen) return;
if (msg && g_update_label) { ... }
```

**Pointer zeroing after nulling:** After `lv_obj_clean`, explicitly zero the child pointer arrays:
```cpp
memset(g_btns, 0, sizeof(g_btns));
memset(g_btn_labels, 0, sizeof(g_btn_labels));
memset(g_btn_icons, 0, sizeof(g_btn_icons));
g_grid = nullptr;
```

## Screen Management Pattern

The overall pattern for navigating to a new screen:
1. Call `lv_scr_load(target_screen)` first (safe to call even before old screen is deleted)
2. Schedule `lv_obj_del_async` on the previous screen pointer (only from within callbacks)
3. Set the old pointer to `nullptr` immediately

From outside callbacks (in `create_*` functions called directly):
1. Synchronously `lv_obj_del` the old screen
2. Allocate new screen with `lv_obj_create(NULL)`
3. Call `lv_scr_load(new_screen)`

## Static Local Variables for State Persistence

Stateful polling functions use `static` locals inside the function body to avoid global scope pollution. Pattern seen in `src/ble_actions.cpp` and `src/webserver.cpp`:
```cpp
void check_ble_status() {
    static bool was_connected = false;
    static unsigned long last_check = 0;
    static unsigned long last_connect_time = 0;
    static uint8_t rapid_disconnect_count = 0;
    // ...
}

void check_wifi_status() {
    static bool was_connected = false;
    static unsigned long last_wifi_check = 0;
    // ...
}
```

## `pt_display.h` Inline Function Pattern

All display functions are `inline` in the header (no separate `.cpp` for the public API). Internal helpers that should not be called by other TUs are `static`. The distinction:

- `inline` = part of the public display API, safe to call from any TU that includes the header
- `static` = internal implementation detail (e.g., `pt_get_duty_from_percent`, `pt_init_backlight`)

This is intentional: the header is the entire display driver; there is no `pt_display.cpp` containing public symbols.

The exception is `pt_display.cpp` which exists but holds only the external variable definitions (`pt_touchpanel`, `pt_rgbpanel`, `pt_gfx`, `pt_disp_draw_buf`, `pt_disp_draw_buf2`, `pt_display_suspended`, `pt_last_touch_ms`).

## Localization Pattern

All UI strings are obtained via `get_l10n()` which returns a `const L10n*` pointing to one of two static `L10n` structs (`g_l10n_en` or `g_l10n_es`) selected by `g_kb_lang`. Defined in `src/l10n.cpp`, declared in `src/l10n.h`.

Usage convention: assign once at the top of a function and use the pointer throughout:
```cpp
void create_settings_ui() {
    const L10n* l = get_l10n();
    lv_label_set_text(title, l->settings_title);
    lv_obj_t* bg_btn = lv_list_add_btn(list, "\xEF\x80\xBE", l->global_bg);
    // ...
}
```

LVGL font icons are embedded as UTF-8 byte sequences inline in string literals (not abstracted). The icon for a symbol is always a raw `"\xEF\x..."` prefix immediately before the label text.

## Settings Persistence (NVS Keys)

`Preferences` namespace `"deck"` (`src/storage.cpp`). Key names are short strings (max 15 chars for NVS key limit):

| Global variable | NVS key |
|-----------------|---------|
| `g_rows` | `"rows"` |
| `g_cols` | `"cols"` |
| `g_target_os` | `"os"` |
| `g_kb_lang` | `"lang"` |
| `g_bg_color` | `"bg"` |
| `g_brightness` | `"bright"` |
| `g_num_pages` | `"num_pages"` |
| `g_sleep_timeout` | `"sleep_to"` |
| `g_current_page` | `"page"` |
| `g_wifi_ssid` | `"wssid"` |
| `g_wifi_pass` | `"wpass"` |

Button configs are stored as raw binary files in LittleFS:
- `/win_btns_v2.bin` — Windows buttons (all pages, `sizeof(ButtonConfig) * MAX_TOTAL_BUTTONS`)
- `/mac_btns_v2.bin` — macOS buttons
- `/linux_btns_v2.bin` — Linux buttons

`save_settings(bool saveButtons = true)` — pass `false` to skip LittleFS write when only NVS settings changed.

## Memory Allocation

- LVGL objects: always via LVGL API (`lv_obj_create`, etc.) — never raw `malloc`
- Context structs passed as event user data: `malloc`/`free` pair, always freed inside the callback that receives it
- Large arrays: use `heap_caps_malloc` with `MALLOC_CAP_SPIRAM` for PSRAM when possible (display buffers in `pt_setup_display`)
- `ButtonConfig` migration buffers: explicit `malloc`/`free` in `storage.cpp`

## Comments Language

Code identifiers and API: English.
Inline explanatory comments: English.
No Portuguese comments exist in the current codebase (the project was described as having some, but all observed comments are English).

Comments explain non-obvious hardware behaviour and intentional workarounds:
```cpp
// The library defaults to ESP_LE_AUTH_REQ_SC_MITM_BOND (MITM + Secure Connections).
// When the ESP32 loses bonding info (e.g. after OTA)...
// ESP_LE_AUTH_BOND (no MITM) is sufficient for a local HID device...
```

## Serial Debug Output Format

Module-prefixed tags on `Serial.println`/`Serial.printf`:
- `"STORAGE: ..."` — storage.cpp events
- `"WEB API: ..."` — webserver.cpp save/restore
- `"RESTORE: ..."` — webserver.cpp restore endpoint (verbose per-field)
- `"OTA: ..."` — OTA progress
- `"BLE ..."` — BLE connection state (no colon, full sentence)
- `"WiFi ..."` — WiFi state (no colon, full sentence)

---

*Convention analysis: 2026-05-13*
