# Roadmap: PandaTouch StreamDeck

## Overview

Starting from a stable v1.8.0 base, this roadmap takes PandaTouch StreamDeck from a private working firmware to a polished public v1.0.0 open-source release. Phase 1 closes out the in-progress UI stability work. Phases 2 and 3 add the two new button types (Text Macro and Home Assistant HTTP). Phase 4 completes the web dashboard and ships the release.

## Phases

- [ ] **Phase 1: UI Stability & Bug Fixes** - Close out the settings refactor, fix known crashes and screen leaks, verify disabled button state
- [ ] **Phase 2: Text Macro Button Type** - Implement BTN_TYPE_TEXT — full string typing via BLE with special character support
- [ ] **Phase 3: Home Assistant HTTP Button Type** - Implement BTN_TYPE_HTTP — non-blocking GET/POST to HA endpoints
- [ ] **Phase 4: Web Dashboard Complete & Release** - All button types in web UI, image library UX, README/CHANGELOG, version bump to v1.0.0

## Phase Details

### Phase 1: UI Stability & Bug Fixes
**Goal**: The firmware is crash-free and the settings experience is fully usable before any new features land
**Depends on**: Nothing (first phase)
**Requirements**: R1.1, R1.2, R1.3
**Success Criteria** (what must be TRUE):
  1. Settings screen scrolls through all 8 items without triggering any action on release
  2. Button list opens on its own screen, scrolls freely, and returns cleanly — no crash on repeated open/close cycles
  3. Navigating into and out of settings multiple times in a session does not grow heap usage (no screen memory leak)
  4. A Disabled button on the main deck is visually grayed out and produces no BLE output when tapped
  5. Sleep overlay bug is resolved: leaving the device on the settings screen past the timeout does not permanently disable sleep
**Plans**: TBD
**Effort**: 2-3 days

### Phase 2: Text Macro Button Type
**Goal**: Users can configure a button that types a complete text string via BLE when pressed
**Depends on**: Phase 1
**Requirements**: R2.1, R2.2, R2.3
**Success Criteria** (what must be TRUE):
  1. A button set to "Text Macro" type and configured with a string types all characters to the connected host when pressed
  2. A multi-line macro (containing `\n`) sends an ENTER keypress at each newline; `\t` sends TAB
  3. Typing a 50-character string completes without dropping characters on Windows, macOS, and Linux
  4. The button is configurable from the device settings screen (Action = "Text Macro", textarea for the string)
  5. A text macro button configured via the web dashboard saves and behaves identically to one configured on-device
**Plans**: TBD
**Effort**: 2-3 days
**UI hint**: yes

### Phase 3: Home Assistant HTTP Button Type
**Goal**: Users can configure a button that fires an HTTP GET or POST to a Home Assistant endpoint when pressed, without freezing the UI or interrupting BLE
**Depends on**: Phase 2
**Requirements**: R3.1, R3.2, R3.3, R3.4
**Success Criteria** (what must be TRUE):
  1. A button set to "HTTP Request" type triggers the configured URL when pressed and the request completes within 3 seconds
  2. The device display remains responsive during the HTTP request — LVGL does not freeze
  3. Pressing an HTTP button while WiFi is disconnected produces no crash and no visible error (silent no-op)
  4. A failed request (timeout or server error) does not lock up the device or leave it in a bad state
  5. The button is fully configurable from the web dashboard with GET/POST toggle and an optional request body field
**Plans**: TBD
**Effort**: 2-3 days
**UI hint**: yes

### Phase 4: Web Dashboard Complete & Release
**Goal**: The web dashboard supports every button type with a polished UX, and the project is packaged for public open-source release as v1.0.0
**Depends on**: Phase 3
**Requirements**: R4.1, R4.4, R5.1, R5.2, R5.3, R5.4
**Success Criteria** (what must be TRUE):
  1. The web dashboard type selector includes Text Macro (with textarea and `\n`/`\t` hint) and HTTP Request (with URL, GET/POST toggle, body field)
  2. The image library panel lets a user upload an image, preview its filename in the button editor, and delete it — system files are never shown or deletable
  3. A new PandaTouch owner can read README.md and go from blank device to working Stream Deck without asking questions
  4. CHANGELOG.md documents all changes from v1.7.x to v1.0.0 in standard semantic versioning format
  5. `constants.h` reports `PANDA_VERSION "1.0.0"` and a `v1.0.0` git tag exists on the release commit
**Plans**: TBD
**Effort**: 2-3 days
**UI hint**: yes

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. UI Stability & Bug Fixes | 0/TBD | Not started | - |
| 2. Text Macro Button Type | 0/TBD | Not started | - |
| 3. Home Assistant HTTP Button Type | 0/TBD | Not started | - |
| 4. Web Dashboard Complete & Release | 0/TBD | Not started | - |

---

## Requirement Coverage

| Requirement | Phase | Status |
|-------------|-------|--------|
| R1.1 — Settings menu fully usable | Phase 1 | Pending |
| R1.2 — No LVGL screen memory leaks | Phase 1 | Pending |
| R1.3 — Button disabled state complete | Phase 1 | Pending |
| R2.1 — New button type: BTN_TYPE_TEXT | Phase 2 | Pending |
| R2.2 — Character delay | Phase 2 | Pending |
| R2.3 — Special character support | Phase 2 | Pending |
| R3.1 — New button type: BTN_TYPE_HTTP | Phase 3 | Pending |
| R3.2 — Non-blocking execution | Phase 3 | Pending |
| R3.3 — WiFi dependency handled gracefully | Phase 3 | Pending |
| R3.4 — Web dashboard support (HTTP type) | Phase 3 | Pending |
| R4.1 — All button types in web UI | Phase 4 | Pending |
| R4.2 — Page count control in web UI | Already done (v1.8.0) | - |
| R4.3 — Sleep timeout control in web UI | Already done (v1.8.0) | - |
| R4.4 — Image library UX | Phase 4 | Pending |
| R5.1 — README.md | Phase 4 | Pending |
| R5.2 — CHANGELOG.md | Phase 4 | Pending |
| R5.3 — Flash instructions | Phase 4 | Pending |
| R5.4 — Version bump to v1.0.0 | Phase 4 | Pending |

**Coverage: 16/16 v1 requirements mapped (R4.2 and R4.3 already shipped)**

---

## Architecture Notes for Implementers

These concerns from the codebase audit are relevant to implementation decisions across phases:

- **New button types** require changes in exactly four places: `ButtonType` enum in `constants.h`, dispatch in `ble_actions.cpp`, edit screen dropdown in `ui_settings.cpp`, and web dashboard JS in `webserver_html.h`. Missing any one causes silent type mismatch.
- **HTTP requests in Phase 3** must not use `delay()` in the button callback. Use a FreeRTOS task or async pattern — the existing async webserver (ESPAsyncWebServer) pattern is the reference.
- **Phase 1 sleep overlay bug** is in `src/streamdeck.cpp` lines 24-53: `enter_sleep()` must check `lv_scr_act() == g_main_screen` before attaching the overlay, or `back_to_main_cb` must detect and destroy a stale overlay on return.
- **Phase 1 double-tap crash** in `ui_helpers.cpp` lines 10-21: set `ctx->screen = nullptr` before `lv_obj_del()` and guard with `if (!ctx->screen) return;` at entry.
- **Backup/restore v1 bug** (CONCERNS.md) is known tech debt — fixing it is out of v1.0 scope unless it surfaces as a blocker.
