---
phase: 01-ui-stability-bug-fixes
verified: 2026-05-14T18:00:00Z
status: human_needed
score: 5/6 must-haves verified
overrides_applied: 0
human_verification:
  - test: "Sleep overlay does not fire in settings; fires and repeats from main screen"
    expected: "Leaving device on Settings screen past sleep timeout leaves display ON. Returning to main and waiting dims the display. Touching wakes it. Waiting again dims it again."
    why_human: "Requires flashing firmware and observing hardware display behavior — not verifiable from source alone"
  - test: "No heap growth on repeated settings traversal"
    expected: "Cycling Main > Settings > sub-screen > Back x5 shows no heap decrease beyond ~5 KB drift on serial monitor"
    why_human: "Requires running firmware on device with serial monitor — runtime heap behavior cannot be verified statically"
  - test: "Button type Disabled appears grayed out on main deck and sends no BLE output"
    expected: "Button with BTN_TYPE_DISABLED shows LV_STATE_DISABLED visual style; tapping it produces no HID report on connected host"
    why_human: "Visual state and BLE transmission require live device and a connected BLE host"
---

# Phase 1: UI Stability & Bug Fixes — Verification Report

**Phase Goal:** Close critical UI stability bugs — disabled button BLE suppression, settings menu scroll-free layout, button-list sub-screen navigation, no memory leaks on settings traversal, sleep overlay safe in settings, RGB slider vertical scroll isolation.

**Verified:** 2026-05-14T18:00:00Z
**Status:** HUMAN_NEEDED — 5/6 code-level truths VERIFIED; 3 behaviors require device confirmation
**Re-verification:** No — initial verification (includes post-review fixes CR-01, CR-02, CR-03, WR-01, WR-02)

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|---------|
| 1 | Disabled button appears grayed out on main deck and produces no BLE action | VERIFIED (partial) | `ui_main.cpp:151-152` — `lv_obj_add_state(btn, LV_STATE_DISABLED)` on `BTN_TYPE_DISABLED`; `ble_actions.cpp:171` — early return `if (cfg.type == BTN_TYPE_DISABLED) return;`. Visual + BLE suppression wired. Device confirmation needed. |
| 2 | Settings menu displays exactly 8 items without requiring scroll | VERIFIED | `ui_settings.cpp:83-105` — exactly 8 `lv_list_add_btn(list, ...)` calls inside `create_settings_ui`; `LV_OBJ_FLAG_SCROLLABLE` cleared on screen (line 70); `LV_OBJ_FLAG_SCROLL_CHAIN_VER` cleared on list (line 81); list sized `600x360`. |
| 3 | Button list opens on a separate screen, scrolls freely, and returns to settings without crash | VERIFIED | `create_button_list_ui()` creates `g_button_list_screen = lv_obj_create(NULL)` (line 388 — NULL-parented root screen); Back button registers `back_to_settings_cb` (line 417); `back_to_settings_cb` (lines 422-427) does `lv_obj_del_async + nullptr` then `lv_scr_load(g_settings_screen)` guarded by null-check (WR-02 fix). CR-01 page offset fix verified: `page_offset = g_current_page * BUTTONS_PER_PAGE` applied at line 403. |
| 4 | Navigating into and out of settings multiple times does not grow heap (no LVGL screen leak) | VERIFIED (code-level) | All three screen-exit callbacks use `lv_obj_del_async + nullptr` pattern: `back_to_main_cb` (lines 472-474), `save_edit_cb` (lines 561-562), `save_wifi_cb` (line 495), `back_to_settings_cb` (line 423). No synchronous `lv_obj_del` inside any `_cb` function except the intentional orphan overlay cleanup (sync is safe there — not deleting self). Runtime heap confirmation requires device. |
| 5 | Sleep overlay is safe in settings: does not permanently disable sleep | VERIFIED (code-level) | `enter_sleep()` in `streamdeck.cpp:30` — `if (lv_scr_act() != g_main_screen) return;` prevents overlay creation on any non-main screen. Overlay always parented to `g_main_screen` (line 36). `back_to_main_cb` defensively detects and synchronously destroys any overlay whose parent is not `g_main_screen` (lines 465-469). Device timeout behavior requires hardware confirmation. |
| 6 | RGB sliders in edit screen do not propagate vertical gesture to parent screen | VERIFIED | `ui_settings.cpp:215` — `lv_obj_clear_flag(*slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER)` present inside `create_rgb_slider` lambda, applied to all three sliders (R, G, B) at lines 219-221. |

**Score: 5/6 truths fully verifiable from code** (truths 1, 4, 5 have code evidence complete; device confirmation needed for behavioral side)

---

### Deferred Items

None.

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/streamdeck.cpp` | `enter_sleep()` guard + overlay parented to `g_main_screen` | VERIFIED | Line 30: `if (lv_scr_act() != g_main_screen) return;` Line 36: `lv_obj_create(g_main_screen)`. Old `lv_obj_create(lv_scr_act())` is absent. `g_sleep_overlay` non-static (no `static` keyword at line 16). |
| `src/streamdeck.h` | `extern lv_obj_t* g_sleep_overlay;` | VERIFIED | Line 13: `extern lv_obj_t* g_sleep_overlay;` with explanatory comment block. `#include <lvgl.h>` present. |
| `src/ui_settings.cpp` | RGB slider `SCROLL_CHAIN_VER` cleared; `g_sleep_overlay` zeroed at all `create_main_ui()` call sites; orphan overlay detection in `back_to_main_cb` | VERIFIED | Line 215: `lv_obj_clear_flag(*slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER)`. Seven `g_sleep_overlay = nullptr` zeroing points (lines 318, 330, 342, 378, 467, 476, 563). Line 465: orphan check `lv_obj_get_parent(g_sleep_overlay) != g_main_screen`. |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `streamdeck.cpp::enter_sleep` | `g_main_screen` | `lv_scr_act() != g_main_screen` guard + `lv_obj_create(g_main_screen)` | WIRED | Line 30 guard confirmed; line 36 parent confirmed. |
| `ui_settings.cpp::back_to_main_cb` | `g_sleep_overlay` | `lv_obj_get_parent(g_sleep_overlay) != g_main_screen` before `lv_obj_del_async` | WIRED | Line 465-469 confirmed. |
| `ui_settings.cpp::grid_selected` | `create_main_ui` | `g_sleep_overlay = nullptr` immediately before call | WIRED | Line 318 confirmed. |
| `ui_settings.cpp::os_selected` | `create_main_ui` | `g_sleep_overlay = nullptr` immediately before call | WIRED | Line 330 confirmed. |
| `ui_settings.cpp::pages_selected` | `create_main_ui` | `g_sleep_overlay = nullptr` immediately before call | WIRED | Line 378 confirmed. |
| `ui_settings.cpp::lang_selected` | `create_main_ui` | `g_sleep_overlay = nullptr` immediately before call | WIRED | Line 342 confirmed (CR-03 fix). |
| `ui_settings.cpp::save_edit_cb` | `create_main_ui` | `g_sleep_overlay = nullptr` immediately before call | WIRED | Line 563 confirmed (CR-02 fix). |
| `ble_actions.cpp::handle_button_action` | BLE suppression | `if (cfg.type == BTN_TYPE_DISABLED) return;` | WIRED | Line 171 confirmed. |
| `ui_main.cpp::render_button` | visual disabled state | `lv_obj_add_state(btn, LV_STATE_DISABLED)` | WIRED | Lines 151-152 confirmed. |

---

### Data-Flow Trace (Level 4)

Not applicable — this phase modifies firmware LVGL callbacks and control flow, not data-rendering components with async data sources. All wired links produce observable effects via LVGL state changes and BLE output, not rendered dynamic data.

---

### Behavioral Spot-Checks

Step 7b: SKIPPED — embedded ESP32 firmware; no runnable entry point without flashing hardware. Behavioral verification routed to human verification section.

---

### Probe Execution

Step 7c: No probe scripts found or declared for this phase.

```
find scripts -path '*/tests/probe-*.sh' -type f 2>/dev/null → (no output)
```

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|---------|
| R1.1 | PLAN.md | Settings menu fully usable — 8 items without scroll, button list on separate screen, settings changes apply correctly | SATISFIED | 8-item list verified (lines 83-105); `LV_OBJ_FLAG_SCROLLABLE` cleared; `g_button_list_screen` as NULL-parented root; `back_to_settings_cb` wired; page offset CR-01 fix present |
| R1.2 | PLAN.md | No LVGL screen memory leaks — screens properly deleted on exit | SATISFIED | `lv_obj_del_async + nullptr` in `back_to_main_cb`, `save_edit_cb`, `save_wifi_cb`, `back_to_settings_cb`; no sync `lv_obj_del` inside any event callback (only in `create_*` functions and intentional orphan cleanup) |
| R1.3 | PLAN.md | Button disabled state complete — not clickable, grayed out, no BLE action, default is disabled, web dashboard shows Disabled | SATISFIED | BLE guard at `ble_actions.cpp:171`; visual state at `ui_main.cpp:151-152`; storage default at `storage.cpp:121`; `BTN_TYPE_DISABLED` option in web dashboard `webserver_html.h:307`; `type_disabled` in `l10n.h:20` and `l10n.cpp:7`; RGB slider `SCROLL_CHAIN_VER` fix at `ui_settings.cpp:215` |

All three requirement IDs declared in PLAN.md frontmatter are accounted for and satisfied at code level.

---

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| None | — | — | — | No debt markers (TBD/FIXME/XXX), no stubs, no empty handlers found in phase-modified files. |

Scan results:
- Zero `TBD`, `FIXME`, `XXX`, `HACK`, `PLACEHOLDER` in `streamdeck.cpp`, `streamdeck.h`, `ui_settings.cpp`
- `lv_obj_del` (sync) inside callbacks: only at line 466 inside `back_to_main_cb` — this is intentional and safe (deleting a non-parent overlay before async teardown; comment documents rationale)
- No empty return stubs or hardcoded empty data in any modified file

---

### Human Verification Required

#### 1. Sleep Overlay Behavioral Test

**Test:** Flash firmware; configure Sleep timeout to 30 sec; navigate to Settings; wait >30 sec without touching; observe display.
**Expected:** Display does NOT dim while in Settings. Press Back to return to main screen; wait 30 sec without touching — display DOES dim. Touch screen — display wakes. Wait another 30 sec — display dims again (cycle repeats indefinitely).
**Why human:** `enter_sleep()` guard is code-verified, but the timer interaction at runtime (check_sleep_status → enter_sleep) requires hardware observation to confirm the guard fires correctly in the live task loop and that wake-from-sleep repeats correctly.

#### 2. Heap Stability Under Repeated Navigation

**Test:** Open serial monitor at 115200 baud; cycle: Main → Settings → Edit (any button) → Back → Settings → WiFi → Back → Settings → Button List → Back → Main. Repeat 5-10 times rapidly.
**Expected:** No panic, no stack smashing, no spontaneous restart. Heap reading (via any serial debug or `ESP.getFreeHeap()` call) does not show monotonic decrease beyond ~5 KB total.
**Why human:** `lv_obj_del_async + nullptr` pattern is code-verified, but LVGL deferred delete behavior and actual heap accounting require runtime observation on the ESP32-S3 target.

#### 3. BTN_TYPE_DISABLED Visual and BLE Behavior

**Test:** In Settings, configure Button 1 to type "Disabled"; save; observe main screen. Connect device to a host over BLE; tap Button 1.
**Expected:** Button 1 appears visually grayed out (LVGL disabled style). Tapping it produces no keystroke on the connected host.
**Why human:** `LV_STATE_DISABLED` visual rendering depends on the active LVGL theme/style cascade, which requires display observation. BLE suppression guard is code-verified at `ble_actions.cpp:171` but end-to-end suppression requires a connected host.

---

### Gaps Summary

No blocking code-level gaps. All must-haves are satisfied at the artifact and wiring levels. Three behaviors require device-level confirmation before the phase can be closed as fully passed.

**Post-review fixes confirmed present:**
- CR-01: Page offset applied in `create_button_list_ui` (line 403) — VERIFIED
- CR-02: `g_sleep_overlay = nullptr` before `create_main_ui()` in `save_edit_cb` (line 563) — VERIFIED
- CR-03: `g_sleep_overlay = nullptr` before `create_main_ui()` in `lang_selected` (line 342) — VERIFIED
- WR-01: Explicit null-termination of WiFi credentials in `save_wifi_cb` (lines 486, 488) — VERIFIED
- WR-02: Null-guard on `g_settings_screen` before `lv_scr_load` in `back_to_settings_cb` (line 424) — VERIFIED

---

_Verified: 2026-05-14T18:00:00Z_
_Verifier: Claude (gsd-verifier)_
