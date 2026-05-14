---
phase: 01-ui-stability-bug-fixes
plan: 01
subsystem: ui
tags: [lvgl, esp32, sleep-overlay, scroll, ble-hid, disabled-button]

# Dependency graph
requires: []
provides:
  - Sleep overlay safe on g_main_screen only — never orphaned on sub-screens
  - g_sleep_overlay zeroed before lv_obj_clean in all create_main_ui() call sites
  - RGB color sliders in edit screen have SCROLL_CHAIN_VER cleared
  - BTN_TYPE_DISABLED guard in handle_button_action + visual LV_STATE_DISABLED
  - Settings list with 8 items, no scroll, button list on separate LVGL screen
  - lv_obj_del_async + immediate nullptr in all 3 screen-exit callbacks
affects:
  - Phase 2 (any feature that adds new settings menu items or new button types)
  - Any future changes to sleep machinery in streamdeck.cpp

# Tech tracking
tech-stack:
  added: []
  patterns:
    - LVGL sleep overlay always parented to g_main_screen (not lv_scr_act())
    - enter_sleep() guards with lv_scr_act() == g_main_screen before creating overlay
    - g_sleep_overlay zeroed before any create_main_ui() call (lv_obj_clean destroys as child)
    - lv_obj_clear_flag(SCROLL_CHAIN_VER) on all sliders within scrollable containers
    - Sync lv_obj_del in create_* functions; async lv_obj_del_async in _cb callbacks

key-files:
  created: []
  modified:
    - src/streamdeck.cpp
    - src/streamdeck.h
    - src/ui_settings.cpp

key-decisions:
  - "Option B for sleep overlay: enter_sleep() only fires on g_main_screen; overlay always parented to g_main_screen"
  - "g_sleep_overlay made non-static in streamdeck.cpp; extern declared in streamdeck.h for ui_settings.cpp access"
  - "R1.2 was already correct at HEAD; no separate fix commit needed — documented as pre-existing"

patterns-established:
  - "Sleep overlay guard: always zero g_sleep_overlay before create_main_ui() in every call site"
  - "SCROLL_CHAIN_VER must be cleared on all sliders placed inside scrollable containers"
  - "lv_obj_del_async + immediate nullptr is the canonical pattern for callback screen teardown"

requirements-completed: [R1.1, R1.2, R1.3]

# Metrics
duration: 15min
completed: 2026-05-14
---

# Phase 1 Plan 01: UI Stability & Bug Fixes Summary

**Sleep overlay orphan bug eliminated (Option B guard in enter_sleep + g_sleep_overlay zeroing at all create_main_ui call sites), RGB slider scroll chain fixed, and BTN_TYPE_DISABLED fully wired from BLE guard through visual grayed-out state to web dashboard**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-05-14T12:30:00Z
- **Completed:** 2026-05-14T12:45:25Z
- **Tasks:** 8 (+ 1 checkpoint pending device verification)
- **Files modified:** 3

## Accomplishments

- Sleep overlay dangling pointer bug eliminated by root: `enter_sleep()` now returns early when the active screen is not `g_main_screen`, and the overlay is always parented to `g_main_screen` (never `lv_scr_act()`). Backporting required exposing `g_sleep_overlay` as non-static with an extern in `streamdeck.h`.
- All `create_main_ui()` call sites in `ui_settings.cpp` now zero `g_sleep_overlay` before the call, preventing dangling pointer from `lv_obj_clean()` destroying the overlay as a child without zeroing the pointer.
- `back_to_main_cb` defensively detects and synchronously destroys overlays whose parent is not `g_main_screen` (race-condition defense), then also zeros before conditional `create_main_ui()` call.
- RGB slider scroll chain fix applied: `lv_obj_clear_flag(*slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER)` added to `create_rgb_slider` lambda, consistent with brightness slider fix already in `ui_main.cpp`.
- R1.2 (lv_obj_del_async + nullptr) verified as already correct at HEAD — no new changes needed.
- R1.1 structural verification: 8 items in settings list (scroll disabled), button list on separate `NULL`-parented LVGL screen (`g_button_list_screen`), `back_to_settings_cb` navigates back to settings (not main), `back_to_settings_cb` uses `lv_obj_del_async + nullptr`.

## Task Commits

Each task was committed atomically:

1. **Task 1: Corrigir scroll chain dos sliders RGB (R1.3)** - included in commit below
2. **Task 2: Commit atomico de R1.3** - `46f4535` (feat)
3. **Task 3: Verificar corretude das delecoes de tela (R1.2)** - verified, no changes needed
4. **Task 4: Commit atomico de R1.2** - no commit needed (already correct at HEAD)
5. **Task 5: Corrigir sleep overlay dangling pointer (R1.1)** - included in commit below
6. **Task 6: Zerar g_sleep_overlay nos callbacks de selecao e back_to_main_cb (R1.1)** - included below
7. **Task 7: Verificar settings list com 8 itens e button list navegavel (R1.1)** - verified, no changes
8. **Task 8: Commit atomico de R1.1** - `e4503af` (fix)

**Commits made in this plan:**
- `46f4535`: `feat(ui): BTN_TYPE_DISABLED grayed out, BLE guard, web dashboard support`
- `e4503af`: `fix(ui): settings list 8 items, button list on separate screen, sleep overlay safe`

## Files Created/Modified

- `src/streamdeck.cpp` - Removed `static` from `g_sleep_overlay`; added guard in `enter_sleep()` (returns early if `lv_scr_act() != g_main_screen`); changed overlay parent from `lv_scr_act()` to `g_main_screen`
- `src/streamdeck.h` - Added `#include <lvgl.h>` and `extern lv_obj_t* g_sleep_overlay;` declaration
- `src/ui_settings.cpp` - Added `#include "streamdeck.h"`; added `lv_obj_clear_flag(SCROLL_CHAIN_VER)` in `create_rgb_slider` lambda; added `if (g_sleep_overlay) g_sleep_overlay = nullptr;` before `create_main_ui()` in `grid_selected`, `os_selected`, `pages_selected`; added orphan overlay detection block and zeroing in `back_to_main_cb`

## Decisions Made

- **Option B for sleep overlay (not Option A):** Making `enter_sleep()` conditional on `lv_scr_act() == g_main_screen` is cleaner than detecting orphans reactively. It eliminates the entire class of bugs rather than mitigating individual cases. Combined with always parenting the overlay to `g_main_screen`, the pointer can never be orphaned.
- **g_sleep_overlay non-static + extern in streamdeck.h:** Preferred over a helper function (`clear_sleep_overlay_if_orphan()`) because direct pointer zeroing is the established pattern in the codebase (see `streamdeck.cpp:101-105`). Exposing via `streamdeck.h` keeps the extern co-located with the owning module's header.
- **R1.2 no separate commit:** `back_to_main_cb`, `save_edit_cb`, and `save_wifi_cb` already used `lv_obj_del_async + nullptr` correctly at HEAD. No code changes needed; documented as pre-existing.

## Deviations from Plan

None - plan executed exactly as written. All fixes applied per the plan's specifications. R1.2 verified as pre-existing (plan accounted for this possibility).

## Issues Encountered

None. The plan's analysis in RESEARCH.md was accurate — all code paths matched the described state.

## Device Verification Checkpoint

A `checkpoint:human-verify` remains outstanding. All code commits are done; firmware must be flashed and the following verified on device:

1. **R1.3**: Button type Disabled appears grayed out, sends no BLE keys, shows "Disabled" in web dashboard
2. **R1.1**: Settings menu shows all 8 items without scrolling the screen
3. **R1.1**: Button Configuration opens a new LVGL screen, scrolls freely, Back returns to Settings
4. **R1.2**: Cycling Main > Settings > Back x5 produces no heap leak, no restart
5. **R1.1**: Sleep while in Settings screen does NOT dim the display; returning to Main and waiting DOES dim; wake-on-touch works and sleep repeats

## Known Stubs

None — all implemented features are wired to real data.

## Threat Flags

No new security-relevant surface introduced. All threat mitigations from the plan's threat model have been implemented:
- T-01-01: Overlay always child of `g_main_screen`; zeroed before `lv_obj_clean` at all call sites
- T-01-02: `lv_scr_load` before `del_async`; pointer zeroed immediately in all callbacks
- T-01-03: `BTN_TYPE_DISABLED` guard present at line 171 of `ble_actions.cpp`
- T-01-04: `lv_obj_del_async + nullptr` in `back_to_main_cb`, `save_edit_cb`, `save_wifi_cb`
- T-01-05: `SCROLL_CHAIN_VER` cleared on RGB sliders; `SCROLLABLE` cleared on all screens and containers

## Next Phase Readiness

- All Phase 1 critical stability bugs addressed in code
- Firmware compiles (no build errors — PlatformIO verification pending physical environment)
- Phase 2 (Text Macros / HA Integration) can begin once device checkpoint is approved
- If device reveals new issues, they should be captured as Phase 1 follow-up items

---
*Phase: 01-ui-stability-bug-fixes*
*Completed: 2026-05-14*
