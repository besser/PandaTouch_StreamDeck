---
phase: 01-ui-stability-bug-fixes
fixed_at: 2026-05-14T00:00:00Z
review_path: .planning/phases/01-ui-stability-bug-fixes/01-REVIEW.md
iteration: 1
findings_in_scope: 5
fixed: 5
skipped: 0
status: all_fixed
---

# Phase 01: Code Review Fix Report

**Fixed at:** 2026-05-14
**Source review:** .planning/phases/01-ui-stability-bug-fixes/01-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 5 (3 Critical, 2 Warning)
- Fixed: 5
- Skipped: 0

## Fixed Issues

### CR-01: Button list displays and edits wrong page's buttons

**Files modified:** `src/ui_settings.cpp`
**Commit:** 02322e7
**Applied fix:** In `create_button_list_ui()`, computed `page_offset = g_current_page * BUTTONS_PER_PAGE` before the loop, then used `global_idx = page_offset + i` for both `g_configs[global_idx].label` in the display string and as the event user data passed to `edit_btn_select_cb`. This ensures pages 1+ display and edit the correct button slots.

---

### CR-02: `save_edit_cb` calls `create_main_ui()` without zeroing `g_sleep_overlay`

**Files modified:** `src/ui_settings.cpp`
**Commit:** 7b865ac
**Applied fix:** Added `if (g_sleep_overlay) { g_sleep_overlay = nullptr; }` immediately before `create_main_ui()` at the end of `save_edit_cb`, matching the guard already present in `grid_selected`, `os_selected`, and `pages_selected`.

---

### CR-03: `lang_selected` calls `create_main_ui()` without zeroing `g_sleep_overlay`

**Files modified:** `src/ui_settings.cpp`
**Commit:** ec8dc55
**Applied fix:** Added `if (g_sleep_overlay) { g_sleep_overlay = nullptr; }` immediately before `create_main_ui()` in `lang_selected`, completing the consistent guard pattern across all `create_main_ui()` call sites in the file.

---

### WR-01: `strncpy` for WiFi credentials is missing explicit null termination at boundary

**Files modified:** `src/ui_settings.cpp`
**Commit:** 4165b85
**Applied fix:** Added `g_wifi_ssid[31] = '\0';` after the SSID `strncpy` and `g_wifi_pass[63] = '\0';` after the password `strncpy` in `save_wifi_cb`. This guarantees null termination even when the source string fills the entire buffer.

---

### WR-02: `back_to_settings_cb` calls `lv_scr_load(g_settings_screen)` without a null guard

**Files modified:** `src/ui_settings.cpp`
**Commit:** cd0d959
**Applied fix:** Wrapped `lv_scr_load(g_settings_screen)` in `if (g_settings_screen) { ... }` to guard against undefined behavior if the screen pointer is null due to a silent LVGL allocator failure.

---

_Fixed: 2026-05-14_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
