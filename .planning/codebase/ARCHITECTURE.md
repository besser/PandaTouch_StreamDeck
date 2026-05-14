<!-- refreshed: 2026-05-13 -->
# Architecture

**Analysis Date:** 2026-05-13

## System Overview

```text
┌─────────────────────────────────────────────────────────────────────┐
│                        Arduino Loop (Core 0)                        │
│  setup(): pt_setup_display → StreamDeckApp::setup → pt_set_backlight│
│  loop():  pt_loop_display → StreamDeckApp::loop                     │
└────────┬──────────────────┬──────────────────┬──────────────────────┘
         │                  │                  │
         ▼                  ▼                  ▼
┌─────────────────┐ ┌──────────────┐ ┌─────────────────────────────┐
│  pt_loop_display│ │StreamDeckApp │ │ ESPAsyncWebServer (BG tasks) │
│  lv_task_handler│ │  orchestrator│ │  Runs on its own FreeRTOS   │
│  `src/pt/       │ │ `src/        │ │  task; callbacks are ISR-   │
│  pt_display.h`  │ │ streamdeck.  │ │  safe but touch globals     │
│                 │ │ cpp`         │ │  `src/webserver.cpp`        │
└────────┬────────┘ └──────┬───────┘ └──────────────┬──────────────┘
         │                 │                         │
         ▼                 ▼                         │
┌──────────────────────────────────────────┐         │
│              LVGL Screen Stack           │         │
│  g_main_screen (`src/ui_main.cpp`)       │         │
│  g_settings_screen (`src/ui_settings.cpp`│         │
│  g_edit_screen (`src/ui_settings.cpp`)   │         │
│  selection screens (`src/ui_helpers.cpp`)│         │
│  sleep overlay (static, `streamdeck.cpp`)│         │
└──────────────────────────────────────────┘         │
         │                                           │
         ▼                                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     Global State Layer (storage.h)                  │
│  g_configs[MAX_TOTAL_BUTTONS]  g_bg_color  g_rows/g_cols            │
│  g_target_os  g_current_page   g_num_pages  g_brightness            │
│  g_wifi_status  g_ip_addr  g_kb_lang  g_sleep_timeout               │
└────────┬────────────────────────────────────┬────────────────────────┘
         │                                    │
         ▼                                    ▼
┌──────────────────┐                ┌──────────────────────────────┐
│  NVS Preferences │                │  LittleFS (binary blobs)     │
│  ("deck" ns)     │                │  /win_btns_v2.bin            │
│  scalar settings │                │  /mac_btns_v2.bin            │
│                  │                │  /linux_btns_v2.bin          │
│                  │                │  /user-images (PNG/BMP)      │
└──────────────────┘                └──────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| `pt_display` HAL | LCD init, LVGL init, touch polling, backlight LEDC, OTA lockout | `src/pt/pt_display.h` |
| `StreamDeckApp` | Main loop orchestrator: sleep, BLE poll, WiFi poll, UI rebuild, OTA handoff | `src/streamdeck.cpp` |
| `ui_main` | Main deck screen: button grid, nav bar, brightness slider, page nav | `src/ui_main.cpp` |
| `ui_settings` | Settings, button edit, WiFi, color picker sub-screens | `src/ui_settings.cpp` |
| `ui_helpers` | Generic reusable selection screen widget | `src/ui_helpers.cpp` |
| `ble_actions` | BLE keyboard init, button action dispatch, bond management | `src/ble_actions.cpp` |
| `webserver` | AsyncHTTP REST API, WiFi polling, config JSON, OTA HTTP upload | `src/webserver.cpp` |
| `storage` | NVS read/write, LittleFS binary button files, v1→v2 migration | `src/storage.cpp` |
| `l10n` | US/ES string table, LVGL symbol name↔code mapping | `src/l10n.cpp` |
| `constants` | Compile-time limits, enums: ButtonType, TargetOS, KbLang | `src/constants.h` |

## Pattern Overview

**Overall:** Single-threaded cooperative event loop with LVGL event-driven UI and an async webserver on a FreeRTOS background task.

**Key Characteristics:**
- No RTOS tasks created by application code; `loop()` is the only user scheduling point
- LVGL drives all UI updates; the app never redraws directly to the framebuffer
- Cross-context communication via `volatile` flag variables (`g_pending_ui_update`, `g_ota_screen_requested`) — the webserver ISR sets flags; `StreamDeckApp::loop()` acts on them on the next tick
- All persistent state lives in module-global `extern` variables declared in `storage.h`
- No heap allocation for button data; `g_configs` is a static flat array

## Layers

**HAL Layer:**
- Purpose: Isolate hardware-specific display, touch, and backlight code
- Location: `src/pt/pt_display.h`, `src/pt/pt_board.h`
- Contains: Display driver setup, LVGL flush/read callbacks, backlight LEDC, OTA screen lockout
- Depends on: Arduino GFX Library, TAMC_GT911, LVGL, esp-idf LEDC
- Used by: `src/main.cpp`, `src/streamdeck.cpp`

**Orchestration Layer:**
- Purpose: Coordinate subsystems each loop tick; own sleep/wake state machine
- Location: `src/streamdeck.cpp`
- Contains: `setup()`, `loop()`, sleep overlay lifecycle, OTA handoff
- Depends on: storage, ui_main, ble_actions, webserver, pt_display, ArduinoOTA
- Used by: `src/main.cpp`

**UI Layer:**
- Purpose: Build and manage LVGL screen objects for deck and settings
- Location: `src/ui_main.cpp`, `src/ui_settings.cpp`, `src/ui_helpers.cpp`
- Contains: Screen creation/teardown, event callbacks, button rendering
- Depends on: storage (global state), ble_actions (button dispatch), l10n, pt_display
- Used by: `src/streamdeck.cpp`, each other

**Business Logic Layer:**
- Purpose: BLE keyboard HID actions keyed to button type and OS
- Location: `src/ble_actions.cpp`
- Contains: `handle_button_action()`, `execute_adv_shortcut()`, `check_ble_status()`, bond clearing
- Depends on: storage (g_configs, g_target_os, g_kb_lang), BleKeyboard library
- Used by: `src/ui_main.cpp` (touch), `src/streamdeck.cpp` (via `handle_button`)

**Persistence Layer:**
- Purpose: Load/save scalar settings to NVS; load/save button arrays to LittleFS
- Location: `src/storage.cpp`
- Contains: `init_storage()`, `load_settings()`, `save_settings()`, v1→v2 migration lambda
- Depends on: Preferences (NVS), LittleFS, WiFi (triggers reconnect on load)
- Used by: `src/streamdeck.cpp`, `src/webserver.cpp`, `src/ui_settings.cpp`

**Web API Layer:**
- Purpose: Serve browser UI, expose REST endpoints, handle OTA HTTP upload
- Location: `src/webserver.cpp`, `src/webserver_html.h`
- Contains: `init_webserver()`, `check_wifi_status()`, JSON builders, file upload handlers
- Depends on: ESPAsyncWebServer, ArduinoJson, storage globals, l10n, LittleFS
- Used by: `src/streamdeck.cpp` (polls `check_wifi_status()` each loop)

## Data Flow

### Primary Touch → BLE Keypress

1. Touch panel interrupt → `pt_touchpad_read()` called by LVGL (`src/pt/pt_display.h:209`)
2. LVGL dispatches `LV_EVENT_CLICKED` to button widget
3. `btn_event_cb()` in `src/ui_main.cpp:25` computes `global_idx = (g_current_page * BUTTONS_PER_PAGE) + local_idx`
4. Calls `handle_button_action(global_idx)` in `src/ble_actions.cpp:162`
5. Reads `g_configs[global_idx]` — dispatches based on `ButtonType` enum
6. Sends HID report via BleKeyboard library over BLE

### Web API → UI Refresh

1. Browser POST to `/api/save` → webserver ISR callback (`src/webserver.cpp:169`)
2. Webserver writes directly into `g_configs[]` and scalar globals
3. Calls `save_settings()` then `load_settings()` to normalize state
4. Sets `g_pending_ui_update = true` (volatile flag)
5. Next `StreamDeckApp::loop()` tick detects flag → calls `create_main_ui()` (`src/streamdeck.cpp:96`)
6. LVGL rebuilds all widgets from updated globals

### OS Switch Flow

1. OS change detected: `isOSSwitch = (request->hasParam("os") && request->params() <= 2)` (`src/webserver.cpp:242`)
2. `save_settings(false)` — saves scalars only, skips button write for current OS
3. `load_settings()` — reads `g_target_os`, loads the correct `*_btns_v2.bin` file into `g_configs`
4. `g_pending_ui_update = true` → UI rebuild

### Sleep/Wake

1. `pt_touchpad_read()` updates `pt_last_touch_ms = millis()` on every real touch (`src/pt/pt_display.h:215`)
2. `check_sleep_status()` in `src/streamdeck.cpp:39` compares elapsed time against `timeout_ms_map[g_sleep_timeout]`
3. On timeout: `enter_sleep()` dims backlight to 0%, creates transparent full-screen LVGL overlay
4. First touch lands on overlay → `sleep_overlay_cb()` restores backlight, destroys overlay, resets timer
5. Subsequent touches pass through normally to widgets

**State Management:**
- All configuration state is module-level globals defined in `src/storage.cpp` and declared `extern` in `src/storage.h`
- LVGL widget pointers are static locals within each `ui_*.cpp` file; they become stale after `lv_obj_clean()` — always call the `create_*_ui()` function to rebuild rather than caching references across rebuilds

## Key Abstractions

**ButtonConfig:**
- Purpose: Represents one button's full configuration
- Definition: `src/storage.h:8-15`
- Fields: `label[16]`, `value[256]`, `type` (ButtonType enum), `color` (RGB24), `icon[8]` (LVGL symbol UTF-8), `imgPath[32]`

**g_configs flat array:**
- Purpose: All button data for all pages of all OS targets currently loaded
- Definition: `ButtonConfig g_configs[MAX_TOTAL_BUTTONS]` — `MAX_TOTAL_BUTTONS = MAX_PAGES * BUTTONS_PER_PAGE = 5 * 20 = 100`
- Index formula: `global_idx = page * BUTTONS_PER_PAGE + local_position`
- Only the active OS's file is loaded in memory at any time

**volatile UI flags:**
- `g_pending_ui_update` — webserver/settings set this; `StreamDeckApp::loop()` drains it
- `g_ota_screen_requested` — webserver OTA handler sets this; loop enters lockout mode
- Declared in `src/ui_main.h:10-12`, defined in `src/ui_main.cpp:10-11`

## Entry Points

**`setup()`:**
- Location: `src/main.cpp:7`
- Triggers: Arduino framework power-on
- Responsibilities: CPU clock, GPIO, Serial, display HAL, StreamDeckApp init, backlight

**`loop()`:**
- Location: `src/main.cpp:27`
- Triggers: Arduino framework, called continuously
- Responsibilities: `pt_loop_display()` (LVGL tick) then `StreamDeckApp::loop()` (all subsystem polling)

**`StreamDeckApp::loop()`:**
- Location: `src/streamdeck.cpp:91`
- Calls in order: `check_ble_status()`, `check_wifi_status()`, `check_sleep_status()`, pending-flag draining, `ArduinoOTA.handle()`, `yield()`

## Architectural Constraints

- **Threading:** Single-threaded Arduino loop on Core 0. `ESPAsyncWebServer` runs its own FreeRTOS task internally — its request callbacks execute on a different core/thread. All webserver callbacks that touch globals must only write atomically-sized values or use the `volatile` flag pattern.
- **Global state:** All shared config is module-level globals in `src/storage.cpp`. No locking mechanism exists; the webserver-to-UI path relies on the volatile flag and LVGL's single-thread assumption.
- **LVGL not thread-safe:** `lv_obj_*` calls must only happen from the loop task. The webserver sets flags; the loop acts.
- **LittleFS per-OS files:** Only one OS's button data is live in `g_configs` at a time. Switching OS reloads the array from disk; unsaved changes to the previous OS are lost.
- **OTA lockout:** `pt_enter_ota_mode()` frees LVGL draw buffers and sets `pt_display_suspended = true`, halting `lv_task_handler()`. After this point, only the GFX library's direct draw functions are safe.
- **Circular imports:** None detected. Dependency order is: constants ← storage ← ble_actions/webserver/ui_* ← streamdeck ← main.

## Anti-Patterns

### Webserver callbacks writing directly to g_configs

**What happens:** The `/api/save` handler in `src/webserver.cpp:187` writes directly to `g_configs[i]` fields from the AsyncWebServer task context.
**Why it's wrong:** LVGL and button dispatch also read `g_configs` from the loop task with no mutex. A race between a POST and a touch event could corrupt button data mid-read.
**Do this instead:** Queue the incoming data into a separate buffer and apply it during `StreamDeckApp::loop()` after detecting `g_pending_ui_update`, same pattern already used for screen rebuilds.

### sleep_overlay not guarded during UI rebuild

**What happens:** `create_main_ui()` calls `lv_obj_clean(g_main_screen)` which destroys the sleep overlay's parent, leaving `g_sleep_overlay` non-null but pointing to a deleted object if called from webserver path.
**Why it's wrong:** `streamdeck.cpp:101` checks `g_sleep_overlay != nullptr` and nulls it before the clean, but only in the `g_pending_ui_update` path. A direct `create_main_ui()` call from elsewhere would dangling-ref the overlay pointer.
**Do this instead:** Always null `g_sleep_overlay` before any call to `create_main_ui()`.

## Error Handling

**Strategy:** Silent fallback with Serial logging. No exceptions (C++ exceptions disabled in Arduino/ESP-IDF by default).

**Patterns:**
- Storage: size mismatch on load → `goto load_defaults` resets to blank buttons (`src/storage.cpp:112`)
- LittleFS mount failure → logs to Serial, continues (no buttons will load)
- BLE not connected → `handle_button_action()` returns early with Serial log (`src/ble_actions.cpp:163`)
- Image file missing → silently falls back to icon, then label (`src/ui_main.cpp:121`)
- BLE bond mismatch → `check_ble_status()` counts rapid disconnects, clears bonds after 3 cycles (`src/ble_actions.cpp:62`)

## Cross-Cutting Concerns

**Logging:** `Serial.print/println/printf` throughout. `StreamDeckApp::log()` is a thin `vprintf` wrapper (`src/streamdeck.cpp:123`). No log levels or filtering.
**Validation:** Scalar bounds checked in `load_settings()` after NVS read. Web API validates row/col/page counts inline before assigning globals.
**Authentication:** None. Web UI and API have no authentication. Access is limited by LAN network membership only.

---

*Architecture analysis: 2026-05-13*
