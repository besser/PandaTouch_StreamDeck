# Codebase Structure

**Analysis Date:** 2026-05-13

## Directory Layout

```
PandaTouch_streamDeck/
├── src/                    # All application source files
│   ├── pt/                 # Hardware Abstraction Layer (display + board)
│   │   ├── pt_display.h    # LCD, LVGL init, touch, backlight, OTA lockout (header-only)
│   │   └── pt_board.h      # GPIO pin assignments and LCD timing constants
│   ├── main.cpp            # Arduino setup()/loop() entry point
│   ├── streamdeck.cpp/.h   # StreamDeckApp orchestrator class
│   ├── ui_main.cpp/.h      # Main deck screen (button grid + nav bar)
│   ├── ui_settings.cpp/.h  # Settings, button editor, WiFi, color picker screens
│   ├── ui_helpers.cpp/.h   # Reusable generic selection screen widget
│   ├── ble_actions.cpp/.h  # BLE keyboard init, action dispatch, bond recovery
│   ├── storage.cpp/.h      # NVS + LittleFS persistence, global state definitions
│   ├── webserver.cpp/.h    # ESPAsyncWebServer REST API and WiFi polling
│   ├── webserver_html.h    # Embedded HTML/JS/CSS for browser UI (string literal)
│   ├── l10n.cpp/.h         # Localization string tables (EN/ES) and symbol maps
│   ├── constants.h         # Compile-time limits and shared enums
│   └── pt_demo.h           # (Unused demo scaffold — not referenced by build)
├── include/                # Project-level LVGL config
│   └── lv_conf.h           # LVGL feature flags and font enables
├── docs/
│   └── images/             # README screenshots
├── .github/workflows/      # CI workflow definitions
├── .planning/codebase/     # GSD codebase maps (this directory)
├── .pio/                   # PlatformIO build output (generated, not committed)
├── partitions_custom.csv   # Custom flash partition table (16 MB)
├── platformio.ini          # PlatformIO build configuration
├── platformio.example.ini  # Template for users without WiFi credentials
├── build_files_exclude.py  # Pre-build script for arduino-3x env exclusions
├── AGENTS.md               # AI agent context document
├── FUTURE_IMPROVEMENTS.md  # Planned features backlog
└── CHANGELOG.md            # Version history
```

## Directory Purposes

**`src/`:**
- Purpose: All firmware source code
- Contains: Application logic, UI, HAL, persistence
- Key files: `main.cpp` (entry), `streamdeck.cpp` (orchestrator), `storage.cpp` (state)

**`src/pt/`:**
- Purpose: Board-specific hardware abstraction; isolates hardware from app logic
- Contains: `pt_display.h` (header-only HAL — all functions are `inline` or `static`), `pt_board.h` (pin/timing macros only)
- Note: `pt_display.h` is large (~380 lines) and entirely inline — suitable only for single-include

**`include/`:**
- Purpose: LVGL configuration header required by the LVGL library
- Key file: `lv_conf.h` — controls which LVGL features, fonts, and color formats are compiled in
- Generated: No — manually maintained

## Key File Locations

**Entry Points:**
- `src/main.cpp`: `setup()` and `loop()` — only 31 lines; delegates immediately

**Orchestration:**
- `src/streamdeck.cpp`: Sleep state machine, subsystem polling, OTA handoff, UI rebuild trigger

**Global State (definitions):**
- `src/storage.cpp`: Defines all `g_*` globals (rows, cols, OS, page, configs array, WiFi, brightness, sleep timeout)
- `src/storage.h`: `extern` declarations — include this anywhere globals are needed

**UI Screens:**
- `src/ui_main.cpp`: Main deck — `create_main_ui()`, `refresh_main_ui()`, `show_update_screen()`
- `src/ui_settings.cpp`: Settings list, button editor, WiFi form, color picker
- `src/ui_helpers.cpp`: `create_selection_screen()` — generic scrollable picker overlay

**BLE Actions:**
- `src/ble_actions.cpp`: `handle_button_action()`, `execute_adv_shortcut()`, `check_ble_status()`

**Web Server:**
- `src/webserver.cpp`: `init_webserver()`, `check_wifi_status()`, all `/api/*` route handlers
- `src/webserver_html.h`: Single `const char* HTML_PAGE` string with embedded browser app

**Configuration:**
- `platformio.ini`: Platform, board, library dependencies, build flags
- `include/lv_conf.h`: LVGL feature/font configuration
- `partitions_custom.csv`: Flash layout — OTA + LittleFS partitions

## Naming Conventions

**Files:**
- Snake_case: `ui_main.cpp`, `ble_actions.cpp`, `pt_display.h`
- Prefix `ui_` for LVGL screen modules
- Prefix `pt_` for HAL/board files

**Functions:**
- Snake_case for free functions: `create_main_ui()`, `handle_button_action()`, `load_settings()`
- `PascalCase::method` for the one static class: `StreamDeckApp::setup()`, `StreamDeckApp::loop()`
- LVGL event callbacks end in `_cb`: `btn_event_cb`, `page_nav_cb`, `slider_event_cb`
- Selection callbacks follow `*_selected` pattern: `grid_selected()`, `os_selected()`

**Variables:**
- `g_` prefix for all global state: `g_configs`, `g_bg_color`, `g_current_page`
- `pt_` prefix for HAL globals: `pt_last_touch_ms`, `pt_display_suspended`
- Static screen-scoped LVGL pointers: `g_settings_screen`, `g_edit_screen` (file-static despite `g_` prefix)
- Local LVGL widget pointers: no prefix, short names (`btn`, `label`, `list`)

**Types/Enums:**
- PascalCase structs: `ButtonConfig`, `WifiUIData`, `EditUIData`, `SelectionCtx`
- SCREAMING_SNAKE for enum values: `BTN_TYPE_APP`, `OS_WINDOWS`, `LANG_US`
- Enum type names PascalCase: `ButtonType`, `TargetOS`, `KbLang`

## Global State — Complete List

All globals are defined in `src/storage.cpp` and declared extern in `src/storage.h`:

| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `g_configs` | `ButtonConfig[100]` | zeroed/disabled | All button data for active OS |
| `g_bg_color` | `uint32_t` | `0x121212` | Background color (RGB24) |
| `g_rows` | `uint8_t` | `3` | Grid row count (1–5) |
| `g_cols` | `uint8_t` | `3` | Grid column count (1–5) |
| `g_target_os` | `uint8_t` | `OS_WINDOWS` | Active OS (ButtonType enum value) |
| `g_current_page` | `uint8_t` | `0` | Active page index (0–g_num_pages-1) |
| `g_num_pages` | `uint8_t` | `MAX_PAGES (5)` | Number of configured pages |
| `g_wifi_ssid` | `char[32]` | `""` | WiFi SSID |
| `g_wifi_pass` | `char[64]` | `""` | WiFi password |
| `g_kb_lang` | `uint8_t` | `LANG_US` | Keyboard language |
| `g_brightness` | `uint8_t` | `50` | Backlight % (1–100) |
| `g_sleep_timeout` | `uint8_t` | `0` | Sleep index (0=off, 1–5=30s/60s/2m/5m/10m) |
| `g_wifi_status` | `String` | `"Disconnected"` | Human-readable WiFi state |
| `g_ip_addr` | `String` | `"0.0.0.0"` | Current IP address |

Additional volatile UI flags defined in `src/ui_main.cpp`, declared in `src/ui_main.h`:

| Variable | Type | Purpose |
|----------|------|---------|
| `g_main_screen` | `lv_obj_t*` | Root LVGL screen object, never destroyed |
| `g_wifi_label` | `lv_obj_t*` | Nav bar WiFi label, updated live by webserver |
| `g_pending_ui_update` | `volatile bool` | Webserver sets → loop calls `create_main_ui()` |
| `g_ota_screen_requested` | `volatile bool` | OTA handler sets → loop enters `pt_enter_ota_mode()` |
| `g_ota_progress` | `volatile int` | OTA progress percent (unused in current build) |

HAL globals defined in `src/pt/pt_display.h` (as `extern` declarations with definitions in `pt_display.cpp`):

| Variable | Type | Purpose |
|----------|------|---------|
| `pt_last_touch_ms` | `volatile unsigned long` | Millis of last touch — drives sleep inactivity timer |
| `pt_display_suspended` | `volatile bool` | When true, `lv_task_handler()` is skipped (OTA mode) |
| `pt_disp_draw_buf` | `lv_color_t*` | Primary LVGL render buffer (PSRAM) |
| `pt_disp_draw_buf2` | `lv_color_t*` | Secondary LVGL render buffer for double-buffering |

## Key Data Structures

**`ButtonConfig` (src/storage.h:8-15):**
```cpp
struct ButtonConfig {
    char label[16];    // Display label (null-terminated)
    char value[256];   // Command string (app path, key name, macro)
    uint8_t type;      // ButtonType enum value
    uint32_t color;    // Background color RGB24 (e.g. 0x333333)
    char icon[8];      // LVGL FontAwesome symbol UTF-8 sequence
    char imgPath[32];  // LittleFS path to image (e.g. "/myicon.png")
};
```

**Button index formula:**
```cpp
uint8_t global_idx = (g_current_page * BUTTONS_PER_PAGE) + local_position;
// local_position: 0..(g_rows*g_cols - 1), row-major order
// global_idx: 0..99 (MAX_TOTAL_BUTTONS - 1)
```

**`ButtonType` enum (src/constants.h:13-19):**
```cpp
enum ButtonType {
    BTN_TYPE_APP = 0,       // Win+R / Cmd+Space / Alt+F2 → type value → Enter
    BTN_TYPE_MEDIA,         // Media HID keys (mute, volup, play, etc.)
    BTN_TYPE_BASIC_COMBO,   // Ctrl/Cmd + single character
    BTN_TYPE_ADV_COMBO,     // "CTRL+SHIFT+F5" style string parsed at runtime
    BTN_TYPE_DISABLED       // Button not configured; rendered grayed-out
};
```

**`TargetOS` enum (src/constants.h:21-25):**
```cpp
enum TargetOS { OS_WINDOWS = 0, OS_MACOS, OS_LINUX };
// Controls: button file loaded, app-launch key combo, Ctrl vs Cmd in basic combos
```

**`KbLang` enum (src/constants.h:27-30):**
```cpp
enum KbLang { LANG_US = 0, LANG_ES };
// Controls character remapping in ble_write() for Spanish keyboard layouts
```

**LittleFS button files:**
- `/win_btns_v2.bin` — Windows button array (raw `ButtonConfig[100]`)
- `/mac_btns_v2.bin` — macOS button array
- `/linux_btns_v2.bin` — Linux button array
- Each file is exactly `sizeof(ButtonConfig) * MAX_TOTAL_BUTTONS` bytes (100 × 307 = 30,700 bytes)
- Legacy v1 files (`/win_btns.bin` etc.) are migrated on first boot if v2 not present

## Screen State Machine

LVGL manages a screen stack. Screens are `lv_obj_t*` created with `lv_obj_create(NULL)` and loaded with `lv_scr_load()`.

```
[Main Deck Screen]  ←──────────────────────────────────────────┐
  g_main_screen                                                 │
  (never destroyed; lv_obj_clean() wipes children only)        │
       │ settings_btn_cb                                        │
       ▼                                                        │
[Settings Screen]                                               │
  g_settings_screen                                             │ back_to_main_cb
       │ settings_*_btn_cb                                      │ (lv_scr_load g_main_screen)
       ▼                                                        │
[Selection Screen]  (ephemeral, auto-deleted on pick or back)   │
  created by create_selection_screen()                          │
       │ on callback fires                                       │
       └──► back to g_settings_screen                          │
                                                                │
[Edit Screen]  g_edit_screen                                    │
  (created on demand, deleted on save/back)                     │
       └──► save_edit_cb → back_to_settings_cb ───────────────►┘
                                                     or back_to_main_cb

[Sleep Overlay]  (transparent, full-screen, on g_main_screen)
  created by enter_sleep(), child of lv_scr_act()
  first touch → sleep_overlay_cb → lv_obj_del_async(overlay)

[OTA Screen]  (raw GFX draw, LVGL suspended)
  pt_enter_ota_mode() — one-way; device reboots after OTA completes
```

## Where to Add New Code

**New button type:**
1. Add enum value to `ButtonType` in `src/constants.h`
2. Add dispatch case in `handle_button_action()` in `src/ble_actions.cpp:162`
3. Add UI label string to `L10n` struct in `src/l10n.h` and both locale implementations in `src/l10n.cpp`
4. Add type option to the edit screen dropdown in `src/ui_settings.cpp`

**New global setting:**
1. Add field to `storage.h` as `extern` declaration
2. Define and initialize in `src/storage.cpp`
3. Load from NVS in `load_settings()` and save in `save_settings()` (`src/storage.cpp`)
4. Expose via web API JSON in `build_config_json()` and parse in `/api/save` handler (`src/webserver.cpp`)
5. Add UI control in `create_settings_ui()` (`src/ui_settings.cpp`)

**New REST endpoint:**
- Add `server.on("/api/new-route", ...)` inside `init_webserver()` in `src/webserver.cpp:158`
- Only safe to call after WiFi connect (webserver starts lazily on first connect)

**New settings sub-screen:**
- Follow the pattern in `src/ui_settings.cpp`: create a `static lv_obj_t* g_new_screen`, add a button in `create_settings_ui()`, implement a `create_new_ui()` function
- Use `create_selection_screen()` from `src/ui_helpers.cpp` for simple option lists

**New UI screen (full):**
- Create `src/ui_newscreen.cpp` and `src/ui_newscreen.h`
- Declare a `lv_obj_t* g_new_screen` (static or extern as needed)
- Call `lv_scr_load(g_new_screen)` to navigate to it; store prior screen reference for back navigation
- All LVGL calls must happen only from the Arduino loop task

**User images:**
- Upload via web UI → stored in LittleFS root (e.g. `/myimage.png`)
- Reference in `ButtonConfig.imgPath` as `/myimage.png`
- Loaded in `create_main_ui()` using `lv_image_set_src(img, "L:/myimage.png")` format

## Special Directories

**`.pio/`:**
- Purpose: PlatformIO build artifacts, compiled libraries, firmware binaries
- Generated: Yes
- Committed: No (in `.gitignore`)

**`.planning/codebase/`:**
- Purpose: GSD codebase analysis documents consumed by planning and execution agents
- Generated: Yes (by GSD mapper)
- Committed: Yes

**`docs/images/`:**
- Purpose: Screenshots and diagrams for README
- Generated: No
- Committed: Yes

---

*Structure analysis: 2026-05-13*
