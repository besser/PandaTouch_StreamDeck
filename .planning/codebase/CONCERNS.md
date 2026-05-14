# Codebase Concerns

**Analysis Date:** 2026-05-13

---

## Tech Debt

**Backup/Restore targets v1 files (single-page format):**
- Issue: `/api/backup` reads from `/win_btns.bin` and `/mac_btns.bin` (old v1 paths, `MAX_BUTTONS = 20` entries). The runtime saves to `/win_btns_v2.bin`, `/mac_btns_v2.bin`, `/linux_btns_v2.bin` (100-entry multi-page format). V1 files either do not exist on current devices or hold only the migrated page 1.
- Files: `src/webserver.cpp` lines 287–288, 415–423
- Impact: A backup captured on a v1.7+ device writes only page 1 buttons (or zeros). On restore, `restore_btns()` writes `MAX_BUTTONS = 20` entries to `/win_btns.bin`, then `save_settings(true)` flushes `g_configs` (100 entries) to `/win_btns_v2.bin` — but the `g_configs` load at line 503 reads `sizeof(g_configs) = 32000` bytes from the 6400-byte `/win_btns.bin`, getting a partial read. Pages 2–5 survive in memory from before the restore, which is silently incorrect.
- Fix approach: Point backup at `*_v2.bin` with `MAX_TOTAL_BUTTONS` entries. Update restore to write `*_v2.bin` directly and reload from the same file. Remove the v1 alias entirely once migration is confirmed complete.

**Linux buttons excluded from backup:**
- Issue: `/api/backup` serialises only `win_btns` and `mac_btns`. Linux button configs (`/linux_btns_v2.bin`) are never exported.
- Files: `src/webserver.cpp` lines 287–288
- Impact: A user who has configured Linux shortcuts loses all of them on restore.
- Fix approach: Add a `linux_btns` key in the backup JSON and a matching restore block, mirroring the existing Windows/macOS pattern.

**`webserver_html.h` is inline HTML/JS in a C header:**
- Issue: The entire web dashboard (~339 lines) lives in `src/webserver_html.h` as a `static const char INDEX_HTML[] PROGMEM = R"rawliteral(...)rawliteral"`. There is no build step to minify, validate, or version the HTML/JS independently.
- Files: `src/webserver_html.h`, `src/webserver.cpp` line 761
- Impact: Editing the dashboard requires C-escaping awareness. No linting, no sourcemaps. Long-term maintenance burden grows as the dashboard grows. Every recompile rebuilds the entire binary even for a one-line JS change.
- Fix approach: Serve from LittleFS (`/index.html`) and upload via OTA filesystem flash or the existing `/api/upload` endpoint. This also enables hot updates without reflashing firmware.

**`save_settings()` called on every page navigation:**
- Issue: `page_nav_cb` in `src/ui_main.cpp` (line 38) calls `save_settings(false)` on every left/right page tap. This writes 10 NVS keys to flash on every swipe.
- Files: `src/ui_main.cpp` lines 31–40
- Impact: NVS wear. ESP32 NVS uses wear levelling, but the NVS partition is typically small. Rapid page-flipping (e.g., demo/testing) accelerates wear unnecessarily. The current page could be persisted only on sleep entry or app shutdown.
- Fix approach: Track a `g_page_dirty` flag; save only when the user has been idle for a few seconds or is entering sleep, not on every navigation event.

**`LegacyButtonConfig` struct in storage.cpp is dead code:**
- Issue: `src/storage.cpp` lines 6–13 define `LegacyButtonConfig` with a 128-byte `value` field. It is never instantiated; migration reads directly into `ButtonConfig` (256-byte `value`). The struct is misleading — it implies old binaries used 128 bytes, but the migration code does not use it.
- Files: `src/storage.cpp` lines 6–13
- Fix approach: Remove the struct. Document the v1 binary layout in a comment if historical context is needed.

**`goto` in `load_settings()`:**
- Issue: `src/storage.cpp` lines 115–122 use `goto load_defaults` to handle a file size mismatch. This is the only `goto` in the codebase and creates a non-obvious control flow within a function that also runs auto-migration.
- Files: `src/storage.cpp` line 116
- Fix approach: Refactor to a helper function `reset_configs_to_defaults()` called via an `if` branch.

---

## Known Bugs

**Backup restores partial data into `g_configs`:**
- Symptoms: After restore, buttons on pages 2–5 may show stale pre-restore content or garbage.
- Files: `src/webserver.cpp` lines 499–508
- Trigger: Perform a backup on a v1.7+ device (multi-page), then restore it. The `ff.read()` call reads `sizeof(g_configs) = 32000` bytes from a 6400-byte `/win_btns.bin`, and the storage layer logs "size mismatch, resetting" on next boot because `save_settings(true)` overwrites `/win_btns_v2.bin` with the partially correct `g_configs`.
- Workaround: After restore, navigate to all pages and manually verify/reset buttons on pages 2–5.

**`/api/delete` does not protect v2 system files:**
- Symptoms: A user (or browser bug) can DELETE `/win_btns_v2.bin`, `/mac_btns_v2.bin`, or `/linux_btns_v2.bin` via the web dashboard.
- Files: `src/webserver.cpp` lines 578–596
- Trigger: POST `/api/delete` with `filename=win_btns_v2.bin`. The guard only checks for the v1 names `win_btns.bin` and `mac_btns.bin`.
- Workaround: None; the device silently falls back to empty defaults on next boot.
- Fix: Extend the forbidden list to include all `*_v2.bin` and `*_btns*.bin` patterns.

**Sleep overlay orphaned when switching screens:**
- Symptoms: If the device enters sleep while the user is on the Settings screen (or any non-main screen), `enter_sleep()` attaches `g_sleep_overlay` to `lv_scr_act()` (the settings screen). Returning to the main screen via `lv_scr_load(g_main_screen)` does not destroy the overlay; the overlay stays on the now-inactive settings screen. `check_sleep_status()` sees `g_sleep_overlay != nullptr` and never re-enters sleep. The device never times out again until reboot.
- Files: `src/streamdeck.cpp` lines 24–53, `src/ui_settings.cpp` back_to_main_cb
- Trigger: Leave device on settings screen longer than the configured sleep timeout.
- Fix: `enter_sleep()` should check `lv_scr_act() == g_main_screen` before attaching, or `back_to_main_cb` should detect and destroy a stale overlay.

**`selection_item_cb` shared `ctx` pointer on multi-button list:**
- Symptoms: If two quick taps register on the selection screen (a real risk on a 5-inch capacitive panel), `selection_item_cb` fires twice for the same `ctx`. The first call deletes the screen and frees `ctx`; the second call dereferences the freed pointer.
- Files: `src/ui_helpers.cpp` lines 10–21
- Trigger: Fast double-tap any option in a selection screen (grid, OS, language, pages, sleep).
- Fix: Set `ctx->screen = nullptr` before `lv_obj_del()` and guard with `if (!ctx->screen) return;` at entry, or use `lv_obj_del_async` and clear the cb user data.

---

## Security Considerations

**Web dashboard has no authentication:**
- Risk: Any device on the same WiFi network can read the full config (including WiFi SSID — not password, but the SSID is visible), change all button mappings, upload arbitrary files to LittleFS, perform OTA firmware flash, and delete system files.
- Files: `src/webserver.cpp` — all `server.on()` handlers
- Current mitigation: None. The device is assumed to be on a trusted local network.
- Recommendations: Add HTTP Basic Auth for `/api/save`, `/api/restore`, `/api/delete`, `/api/update`, and `/api/upload`. The SSID read-back in `/api/backup` should be considered acceptable for local use.

**`/api/upload` accepts any filename and content, no size limit:**
- Risk: A malicious actor can fill LittleFS with arbitrary data (exhausting storage), overwrite any existing file including system binaries, or upload a file named to shadow LVGL image paths.
- Files: `src/webserver.cpp` lines 741–755
- Current mitigation: None beyond LittleFS capacity being naturally bounded (~14MB partition).
- Recommendations: Enforce file extension whitelist (e.g. `.png`, `.bin`, `.jpg`), reject filenames matching `*_btns*.bin`, and enforce a per-file size limit.

**WiFi password stored in NVS in cleartext:**
- Risk: NVS is not encrypted on this build. Physical flash access (e.g., via JTAG or `esptool.py read_flash`) reveals WiFi credentials.
- Files: `src/storage.cpp` lines 145–146
- Current mitigation: None.
- Recommendations: Enable NVS encryption via the ESP-IDF partition key or note explicitly in documentation that the device should not be used with sensitive WiFi credentials.

---

## Performance Bottlenecks

**`/api/config` serialises all 100 buttons on every GET:**
- Problem: `build_config_json()` builds a JSON object with 100 button entries on every dashboard load or refresh.
- Files: `src/webserver.cpp` lines 42–72
- Cause: No pagination, no dirty-only delta response.
- Improvement path: Respond with only the active page (20 buttons) plus metadata, or add `?page=N` query support. Full 100-button export can remain for backup purposes only.

**`create_main_ui()` rebuilds the entire widget tree on every page change:**
- Problem: `page_nav_cb` calls `create_main_ui()` which calls `lv_obj_clean(g_main_screen)` and recreates all widgets from scratch. On a 5x3 grid that is 15 buttons plus nav bar, all allocated and styled fresh.
- Files: `src/ui_main.cpp` lines 57–230, `src/ui_main.cpp` lines 31–40
- Cause: No incremental update — only `refresh_main_ui()` (line 232) does in-place updates, but it is not called by the page navigator.
- Improvement path: Call `refresh_main_ui()` for page changes (update `page_offset` and relabel buttons) instead of full rebuild. Reserve `create_main_ui()` for grid layout changes.

**BLE `delay()` calls block the render loop:**
- Problem: `handle_button_action()` uses `delay(150)`, `delay(500)`, `delay(300)`, `delay(200)`, and `delay(5)` per character of typed text. During these delays, `lv_task_handler()` is not called.
- Files: `src/ble_actions.cpp` lines 182–206
- Cause: BLE HID requires inter-key timing; Arduino `delay()` is the simplest approach.
- Improvement path: Use a non-blocking state machine with `millis()` comparisons, or post BLE actions to a FreeRTOS queue processed in a secondary task. At minimum, call `lv_task_handler()` inside long delays to keep the display responsive.

---

## Architectural Concerns

**`g_configs` is a 31.2 KB static array in RAM:**
- Files: `src/storage.cpp` line 15, `src/storage.h` lines 17–18
- Why it matters: `ButtonConfig` (320 bytes aligned) × 100 entries = 32,000 bytes. This is allocated in the default heap (IRAM/DRAM). The ESP32-S3 has ~512 KB DRAM but shares it with the BLE stack, WiFi stack, ArduinoJson documents, and LVGL widget pool. Under peak load (web API save + LVGL redraw), this is a non-trivial constant pressure.
- Safe modification: Move `g_configs` to PSRAM using `heap_caps_malloc(MALLOC_CAP_SPIRAM)` and access it through a pointer. Add a null-check wrapper. Do not place it in a `.bss_psram` section without verifying the linker script.

**`pt_display.h` contains `inline` functions compiled into every translation unit that includes it:**
- Files: `src/pt/pt_display.h` — `pt_disp_flush`, `pt_touchpad_read`, `pt_setup_display`, `pt_loop_display`, `pt_enter_ota_mode`, `pt_set_backlight`, `millis_cb`
- Why it matters: Five `.cpp` files include this header (`main.cpp`, `streamdeck.cpp`, `ui_main.cpp`, `ui_settings.cpp`, `pt_demo.h`). Each gets its own copy of the function body. The `static uint8_t pt_backlight_percent` at line 66 is also per-TU, meaning there are multiple independent copies of this variable. `pt_set_backlight` writes to its local `pt_backlight_percent` — the copy in `streamdeck.cpp` is different from the copy in `ui_main.cpp`. This is a latent data-consistency bug.
- Safe modification: Move all non-trivial function bodies to `src/pt/pt_display.cpp`. Keep only declarations in the header. Declare `pt_backlight_percent` as `extern` in the header and define it once in the `.cpp`.

**Webserver writes to `g_configs[]` and other globals from an async FreeRTOS task without locking:**
- Files: `src/webserver.cpp` lines 192–237, `src/ui_main.cpp` lines 9–11
- Why it matters: `ESPAsyncWebServer` handlers execute on the `async_tcp` FreeRTOS task (core 0 by default on dual-core, or the same core as `loop()` on single-core boards). The `g_configs`, `g_rows`, `g_cols`, `g_bg_color`, and other globals are also read by LVGL callbacks in the main loop. `volatile` only prevents compiler optimisation — it does not provide atomicity or memory ordering on ARM Cortex-M/Xtensa.
- Safe modification: Use a FreeRTOS mutex (`portMUX_TYPE` or `SemaphoreHandle_t`) around any read/write of shared globals in both the webserver handlers and the UI callbacks.

**`lv_conf.h` uses `LV_MEM_SIZE = 64 KB` (LVGL internal pool):**
- Files: `include/lv_conf.h` line 76
- Why it matters: LVGL is configured to use `LV_STDLIB_CLIB` (line 47), meaning it calls system `malloc`/`free` rather than its own pool. `LV_MEM_SIZE` is therefore inactive. This is correct for Arduino/IDF, but the leftover constant creates confusion about LVGL's actual memory budget. Additionally, `LV_CACHE_DEF_SIZE = 128 KB` (line 460) is set for image/draw cache — this is allocated from the system heap on first use and may cause allocation failures if called when heap is fragmented.
- Safe modification: Document that `LV_USE_STDLIB_MALLOC = LV_STDLIB_CLIB` makes `LV_MEM_SIZE` irrelevant. Review `LV_CACHE_DEF_SIZE` — 128 KB is large for a device sharing heap with BLE, WiFi, and ArduinoJson.

---

## Fragile Areas

**`save_settings()` / `load_settings()` round-trip:**
- Files: `src/storage.cpp` lines 40–131, `src/ui_settings.cpp` lines 324–325, 334–335
- Why fragile: Several settings callbacks call `save_settings(false)` immediately followed by `load_settings()`. `load_settings()` re-opens the active binary file and overwrites `g_configs` from disk. If the binary file is absent or truncated (e.g. after a failed write or flash wear), `g_configs` is silently reset to defaults. The caller has no error return to detect this.
- Safe modification: `load_settings()` should return a `bool` indicating success. Callers in `os_selected` and `lang_selected` should not call `load_settings()` at all — those settings do not require reloading the button binary; they only change `g_target_os`/`g_kb_lang` globals.

**`create_main_ui()` after `g_pending_ui_update = true` from webserver:**
- Files: `src/streamdeck.cpp` lines 96–108
- Why fragile: The webserver handler sets `g_pending_ui_update = true` from an async task. The main loop checks the flag and calls `create_main_ui()`. Between the flag set and the check, LVGL may be mid-render. `lv_obj_clean(g_main_screen)` inside `create_main_ui()` deletes all child widgets, which can invalidate LVGL internal iterators if a flush or timer callback is in progress. There is no task notification or deferred callback — just a boolean race.
- Safe modification: Use `lv_async_call()` to schedule `create_main_ui` on the LVGL task safely, or ensure the webserver handler only queues a request that the main loop services at a safe LVGL checkpoint.

**`g_edit_data` pointers become dangling after `lv_obj_del_async(g_edit_screen)`:**
- Files: `src/ui_settings.cpp` lines 29–35, 539–540
- Why fragile: `g_edit_data.ta_label`, `g_edit_data.ta_value`, etc. point to LVGL objects owned by `g_edit_screen`. After `lv_obj_del_async(g_edit_screen)`, those objects are scheduled for deletion but not yet freed. If any code reads `g_edit_data` after the async delete but before the next frame, it accesses freed memory. Currently safe because nothing reads them after `save_edit_cb`, but the pattern is one accidental read away from a crash.
- Safe modification: Null-initialise all `g_edit_data` fields after use, and add null checks before any access.

---

## Scaling Limits

**LVGL heap under simultaneous WiFi + BLE:**
- Current capacity: ~300–350 KB free heap at idle (estimated based on typical ESP32-S3 BLE + WiFi + LVGL baseline).
- Limit: `build_config_json()` allocates a `JsonDocument` for all 100 buttons; on an ESP32-S3 with ArduinoJson v7, this can require 8–16 KB on the stack/heap. If a restore request arrives concurrently (512 KB cap, `malloc(total + 1)`), and LVGL is allocating a large image cache entry, heap fragmentation may cause a `malloc` failure with no recovery path — `ESPAsyncWebServer` silently drops the request.
- Scaling path: Move `g_configs` to PSRAM. Reduce `LV_CACHE_DEF_SIZE`. Add `heap_caps_get_free_size()` logging to `/api/config` response for diagnostics.

**LittleFS upload has no total size guard:**
- Current capacity: LittleFS partition size minus system files (~14 MB).
- Limit: Uploading files until LittleFS is full causes `LittleFS.open(filename, "w")` to fail silently. The upload handler returns 200 OK regardless.
- Scaling path: Check `LittleFS.totalBytes() - LittleFS.usedBytes()` before opening the write handle; return 507 Insufficient Storage if space is inadequate.

---

## Dependencies at Risk

**`ESPAsyncWebServer` + `AsyncTCP` (unpinned git HEAD):**
- Risk: Both are pinned to `https://github.com/me-no-dev/...git` (HEAD). These repos are not actively maintained for ESP-IDF v5.x / arduino-esp32 v3.x. Breaking API changes can appear silently on `pio update`.
- Impact: Compilation failures or runtime crashes after dependency updates.
- Migration plan: Fork the repos at a known-good commit and pin to that commit hash in `platformio.ini`. Consider migrating to `ESP-IDF` native HTTP server (`esp_http_server`) or a maintained fork such as `ESPAsyncWebServer-esphome`.

**`T-vK/ESP32 BLE Keyboard@^0.3.2`:**
- Risk: The library has not been updated for ESP-IDF v5.x BLE API. The project works around this by manually setting security parameters via `esp_gap_ble_api.h` after `bleKeyboard.begin()`, which is fragile against library internal changes.
- Files: `src/ble_actions.cpp` lines 31–35
- Impact: A minor version bump (`^0.3.2`) could change internal BLE initialisation order, breaking the security override.
- Migration plan: Pin to `0.3.2` exactly (remove `^`). Evaluate migrating to `NimBLE` (already scaffolded in `env:pandatouch-arduino-3x`) which has better ESP-IDF v5 support.

**LVGL 9.3.0 — planned upgrade to 9.5:**
- Risk: `FUTURE_IMPROVEMENTS.md` flags this as planned. The API delta between 9.3 and 9.5 is minor but `lv_conf.h` will require review for deprecated/renamed parameters.
- Files: `include/lv_conf.h`, `platformio.ini` lib_deps
- Impact: Low risk per the improvement note, but `pt_display.h` inline functions (`pt_disp_flush`, `pt_touchpad_read`) use `lv_display_t`, `lv_indev_t`, and `lv_disp_flush_ready` — these must be verified against 9.5 headers.

---

## Missing Critical Features

**No watchdog reset in the main LVGL loop:**
- Problem: `StreamDeckApp::loop()` calls `yield()` but does not call `esp_task_wdt_reset()`. The OTA upload handler explicitly calls it, but normal operation does not. If an LVGL operation stalls (e.g., image decode from LittleFS), the watchdog may trigger an unexpected reboot.
- Files: `src/streamdeck.cpp` line 116
- Blocks: Reliable long-term operation without manual reset.

**No error UI for failed BLE actions:**
- Problem: If BLE is disconnected when a button is pressed, `handle_button_action()` returns silently with a Serial log. The user receives no visual feedback.
- Files: `src/ble_actions.cpp` lines 163–165
- Blocks: Usability — users do not know if their button press was ignored.

**Sleep timeout does not pause on settings screens:**
- Problem: `check_sleep_status()` tracks inactivity via `pt_last_touch_ms` (real touch events). If the user is reading the settings list without touching (e.g., paused to think), the display dims and the overlay lands on the settings screen. See also the orphaned overlay bug above.
- Files: `src/streamdeck.cpp` lines 39–53
- Blocks: Clean settings navigation experience.

---

## Test Coverage Gaps

**No automated tests exist:**
- What's not tested: Everything. There is no test framework, no test directory, and no CI pipeline. All validation is manual (flash + observe).
- Files: Entire `src/` tree
- Risk: Regressions in BLE key mapping, NVS serialisation, and LVGL screen lifecycle are only caught if a human happens to exercise the exact code path after a change.
- Priority: High for `src/storage.cpp` (NVS/LittleFS round-trip) and `src/ble_actions.cpp` (key code mapping correctness). These can be unit-tested on host with mocked Arduino APIs.

---

## What to Watch for When Adding Features

1. **New screen types**: Always null-check and `lv_obj_del_async` the old screen pointer before creating a new one. The pattern in `create_edit_ui` (line 123) and `create_wifi_ui` (line 254) is the correct template. Do not reuse a screen object without deleting it first.

2. **New globals read by both webserver and main loop**: Mark them `volatile` as a minimum. For anything larger than a single byte, add a FreeRTOS mutex. The current `g_pending_ui_update` / `g_ota_screen_requested` flags are safe because they are single-byte booleans written from one task and read from another.

3. **New NVS keys in `save_settings()`**: Each call to `preferences.begin("deck", false)` / `putX()` / `preferences.end()` is a flash write transaction. Add new keys only if the data cannot be derived at boot. Avoid storing data that changes frequently (per-button state, counters).

4. **New button types**: Update `ButtonType` enum in `src/constants.h`, implement in `src/ble_actions.cpp` `handle_button_action()`, update the dropdown options in `src/ui_settings.cpp` `create_edit_ui()`, and update the web dashboard JS in `src/webserver_html.h`. Missing any one of these four places causes silent type mismatch (the numeric enum value is stored, not the name).

5. **Adding more pages or buttons beyond 5×20**: `g_configs` array size is fixed at compile time (`MAX_TOTAL_BUTTONS = 100`). The binary file format is also fixed-size. Increasing these constants invalidates all existing saved button files and requires a new migration path in `load_settings()`.

6. **Any `delay()` > 50ms in an event callback**: This blocks `lv_task_handler()` and freezes the display. Use `millis()`-based non-blocking patterns or post work to a queue.

---

*Concerns audit: 2026-05-13*
