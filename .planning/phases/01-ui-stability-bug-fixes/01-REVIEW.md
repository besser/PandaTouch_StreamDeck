---
phase: 01-ui-stability-bug-fixes
reviewed: 2026-05-14T00:00:00Z
depth: standard
files_reviewed: 3
files_reviewed_list:
  - src/streamdeck.cpp
  - src/streamdeck.h
  - src/ui_settings.cpp
findings:
  critical: 3
  warning: 2
  info: 1
  total: 6
status: issues_found
---

# Phase 01: Code Review Report

**Reviewed:** 2026-05-14
**Depth:** standard
**Files Reviewed:** 3
**Status:** issues_found

## Summary

Three files were reviewed covering the sleep overlay dangling-pointer fix, settings screen scroll, button list sub-screen navigation, and RGB slider scroll chain propagation. The overlay pointer discipline introduced in `streamdeck.cpp` is sound. However, two call sites in `ui_settings.cpp` that directly call `create_main_ui()` do not zero `g_sleep_overlay` beforehand — exactly the class of bug this phase was meant to eliminate. A third critical defect exists in `create_button_list_ui()`: it indexes `g_configs` without applying `g_current_page * BUTTONS_PER_PAGE`, so on any page other than page 0 the button list displays and edits the wrong buttons entirely.

---

## Critical Issues

### CR-01: Button list displays and edits wrong page's buttons

**File:** `src/ui_settings.cpp:402-406`

**Issue:** `create_button_list_ui()` iterates `i` from `0` to `btn_count - 1` and reads `g_configs[i].label` for display, then passes `i` as user data to `edit_btn_select_cb`. `edit_btn_select_cb` stores `i` into `g_editing_idx` (line 449-450), and `save_edit_cb` reads/writes `g_configs[g_editing_idx]` without any page offset. The result: when the user is on page 2 or higher, the button list renders page-1 labels for all entries, and saving edits overwrites page-1 slots instead of the visible page's slots. The bug is silent — no crash, just corrupted data on pages other than page 0.

**Fix:**
```cpp
// In create_button_list_ui(), apply the page offset when reading labels:
int btn_count = g_rows * g_cols;
uint8_t page_offset = g_current_page * BUTTONS_PER_PAGE;
for (int i = 0; i < btn_count; i++) {
    uint8_t global_idx = page_offset + i;
    char buf[64];
    sprintf(buf, "%s %d: %s",
            (g_kb_lang == LANG_ES ? "Boton" : "Button"),
            (i + 1),
            g_configs[global_idx].label);
    lv_obj_t* btn = lv_list_add_btn(list, "\xEF\x8C\x84", buf);
    // Pass global_idx so edit_btn_select_cb → g_editing_idx is page-aware:
    lv_obj_add_event_cb(btn, edit_btn_select_cb, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)global_idx);
}
```

---

### CR-02: `save_edit_cb` calls `create_main_ui()` without zeroing `g_sleep_overlay`

**File:** `src/ui_settings.cpp:551-556`

**Issue:** `save_edit_cb` calls `create_main_ui()` at line 556 without first checking and zeroing `g_sleep_overlay`. `create_main_ui()` calls `lv_obj_clean(g_main_screen)` internally, which destroys the overlay as a child without zeroing the module-level pointer. After the call, `g_sleep_overlay` is a dangling pointer. The next call to `check_sleep_status()` or `sleep_overlay_cb()` will dereference freed memory. This is the same class of defect the phase set out to fix, and was correctly handled in `grid_selected`, `os_selected`, and `pages_selected` (lines 318, 330, 377) but missed here.

**Fix:**
```cpp
save_settings();
g_editing_bg = false;
lv_scr_load(g_main_screen);
if (g_edit_screen)        { lv_obj_del_async(g_edit_screen);        g_edit_screen        = nullptr; }
if (g_button_list_screen) { lv_obj_del_async(g_button_list_screen); g_button_list_screen = nullptr; }
// Add this guard before create_main_ui():
if (g_sleep_overlay) { g_sleep_overlay = nullptr; }  // lv_obj_clean will destroy it as child
create_main_ui();
```

---

### CR-03: `lang_selected` calls `create_main_ui()` without zeroing `g_sleep_overlay`

**File:** `src/ui_settings.cpp:334-343`

**Issue:** Same class of defect as CR-02. `lang_selected` calls `create_main_ui()` at line 342 after `lv_scr_load(g_main_screen)` but without zeroing `g_sleep_overlay`. Since changing the keyboard language goes through a selection screen (not the settings screen guard in `enter_sleep()`), a race is possible in theory, but more importantly the pattern is inconsistent with every other `create_main_ui()` call site in the file. The dangling pointer risk is identical.

**Fix:**
```cpp
static void lang_selected(const char* txt) {
    if (strstr(txt, "English")) g_kb_lang = LANG_US;
    else if (strstr(txt, "Espanol")) g_kb_lang = LANG_ES;

    save_settings(false);
    load_settings();
    g_settings_needs_rebuild = true;
    lv_scr_load(g_main_screen);
    if (g_sleep_overlay) { g_sleep_overlay = nullptr; }  // lv_obj_clean will destroy it as child
    create_main_ui();
}
```

---

## Warnings

### WR-01: `strncpy` for WiFi credentials is missing explicit null termination at boundary

**File:** `src/ui_settings.cpp:480-481`

**Issue:** `g_wifi_ssid` is `char[32]` and `g_wifi_pass` is `char[64]`. The calls are:
```cpp
strncpy(g_wifi_ssid, lv_textarea_get_text(g_wifi_data.ta_ssid), 31);
strncpy(g_wifi_pass, lv_textarea_get_text(g_wifi_data.ta_pass), 63);
```
`strncpy` does not write a null terminator when the source is at least as long as the count. If the user types exactly 31 characters for the SSID, `g_wifi_ssid[31]` is never set to `'\0'`, leaving an unterminated string. `WiFi.begin(g_wifi_ssid, g_wifi_pass)` then reads past the buffer. No `lv_textarea_set_max_length` is set on `ta_ssid` or `ta_pass` in `create_wifi_ui()` (only `ta_value` in the edit screen has it at line 178), so a user can freely type more than 31/63 characters. The fix is to add explicit null termination, or to set max lengths on the textarea widgets.

**Fix:**
```cpp
strncpy(g_wifi_ssid, lv_textarea_get_text(g_wifi_data.ta_ssid), 31);
g_wifi_ssid[31] = '\0';
strncpy(g_wifi_pass, lv_textarea_get_text(g_wifi_data.ta_pass), 63);
g_wifi_pass[63] = '\0';
```
Or add in `create_wifi_ui()`:
```cpp
lv_textarea_set_max_length(g_wifi_data.ta_ssid, 31);
lv_textarea_set_max_length(g_wifi_data.ta_pass, 63);
```

---

### WR-02: `back_to_settings_cb` calls `lv_scr_load(g_settings_screen)` without a null guard

**File:** `src/ui_settings.cpp:419-422`

**Issue:** `back_to_settings_cb` is registered as a click callback in `create_button_list_ui()`, which itself is only reachable from the settings screen. In normal operation `g_settings_screen` will be non-null. However the function does not verify this before passing the pointer to `lv_scr_load()`. If `g_settings_screen` is null (e.g., the LVGL allocator failed silently when creating it), `lv_scr_load(nullptr)` results in undefined behavior on LVGL 9.x. A null check costs nothing on an embedded target.

**Fix:**
```cpp
static void back_to_settings_cb(lv_event_t* e) {
    if (g_button_list_screen) {
        lv_obj_del_async(g_button_list_screen);
        g_button_list_screen = nullptr;
    }
    if (g_settings_screen) {
        lv_scr_load(g_settings_screen);
    }
}
```

---

## Info

### IN-01: Redundant `g_settings_needs_rebuild = false` assignment in `back_to_main_cb`

**File:** `src/ui_settings.cpp:473`

**Issue:** `back_to_main_cb` checks `g_settings_needs_rebuild` at line 470, calls `create_main_ui()` at line 472, then sets `g_settings_needs_rebuild = false` at line 473. But `create_settings_ui()` already sets `g_settings_needs_rebuild = false` (line 114) when it rebuilds the screen — and the rebuild is what triggered `g_settings_needs_rebuild = true` in the first place (e.g., `lang_selected` at line 340). The assignment at line 473 is never reached in a context where it has any effect. Dead code that could mislead future maintainers into thinking `back_to_main_cb` is the authoritative reset point.

**Fix:** Remove line 473 (`g_settings_needs_rebuild = false;`) from `back_to_main_cb`. The flag is already managed by `create_settings_ui()`.

---

_Reviewed: 2026-05-14_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
