# Requirements — PandaTouch StreamDeck

> Scope: v1.0 public release. Everything needed to ship a stable, feature-complete open-source firmware for PandaTouch owners.

---

## R1 · UI Stability & Polish

**Why**: The settings menu refactor (button list as separate screen) and scroll fixes must land before any new features. Unstable UI blocks everything else.

### R1.1 — Settings menu fully usable
- Settings list (8 items) fits without scroll; no click-during-scroll issue
- Button list opens on separate screen, scrolls freely
- All setting changes (grid, OS, lang, pages, sleep) apply correctly without crashing

### R1.2 — No LVGL screen memory leaks
- `g_edit_screen`, `g_wifi_screen`, `g_button_list_screen` properly deleted on exit
- Device does not restart or go unstable after repeated settings interactions

### R1.3 — Button disabled state complete
- `BTN_TYPE_DISABLED`: not clickable in UI, grayed out visually, no BLE action
- Default buttons initialized as disabled with empty label
- Web dashboard shows "Disabled" option in type selector

---

## R2 · Text Macros

**Why**: The most-requested missing button type — typing complete strings (terminal commands, boilerplate text, passwords).

### R2.1 — New button type: BTN_TYPE_TEXT
- Types the full `value` string character by character via BLE
- Configurable via device settings (Action = "Text Macro") with textarea input
- Configurable via web dashboard with same textarea

### R2.2 — Character delay
- Small inter-character delay (configurable default ~5ms) to prevent buffer overruns on the host
- Works reliably on Windows, macOS, Linux

### R2.3 — Special character support
- Handles newline (`\n` → ENTER), tab (`\t` → TAB) in the string
- Falls back to BLE write for printable ASCII; skips unsupported chars

---

## R3 · Home Assistant HTTP Integration

**Why**: Hybrid HID + IoT controller — buttons trigger automations, scenes, or scripts via HA REST API without any broker.

### R3.1 — New button type: BTN_TYPE_HTTP
- Executes an HTTP GET or POST to a configured URL when pressed
- URL stored in `value` field (up to 255 chars)
- Method (GET/POST) and optional body configured per-button

### R3.2 — Non-blocking execution
- HTTP request runs without freezing the UI or interrupting BLE
- Timeout of 3s max; failure is silent (no crash)

### R3.3 — WiFi dependency handled gracefully
- If WiFi not connected, HTTP button does nothing (same as BLE when disconnected)
- No retry loop that could block the main task

### R3.4 — Web dashboard support
- Type selector shows "HTTP Request"
- Fields for URL (method prefix: `GET:` or `POST:`) and body

---

## R4 · Web Dashboard Complete

**Why**: The web UI is the primary configuration interface for many users. It must support every button type and be polished enough for public release.

### R4.1 — All button types in web UI
- Text Macro: textarea for full string, `\n`/`\t` hint shown
- HTTP Request: URL field, GET/POST toggle, optional body field
- Disabled: type selector option already added; command field hidden

### R4.2 — Page count control in web UI ✓ (done)
- `numPagesSelect` in top bar, saves immediately

### R4.3 — Sleep timeout control in web UI ✓ (done)
- `sleepSelect` in top bar, saves immediately

### R4.4 — Image library UX
- Upload images, preview filename in button editor, delete from library
- File list shows only non-system files

---

## R5 · Release Readiness

**Why**: For public open-source, the repo must be self-explanatory to a new PandaTouch owner.

### R5.1 — README.md
- What it is, hardware requirements, quick-start (flash + connect)
- Screenshot of web dashboard, screenshot of device UI
- Feature list, known limitations

### R5.2 — CHANGELOG.md
- Semantic versioning starting from v1.0.0
- Notable changes from v1.7.x → v1.0.0

### R5.3 — Flash instructions
- PlatformIO command, partition table note, LittleFS upload step

### R5.4 — Version bump to v1.0.0
- `constants.h`: `PANDA_VERSION "1.0.0"`
- Git tag `v1.0.0`

---

## Non-Requirements (explicit exclusions)

| Excluded | Reason |
|---|---|
| MQTT support | HTTP REST covers HA use case without broker |
| NimBLE migration | Bluedroid works; migration is a future option |
| LVGL 9.5 upgrade | Low risk but out of v1.0 scope; add to backlog |
| Mobile app | Web dashboard is sufficient |
| Multi-host BLE | Complex; out of v1.0 |
