# PandaTouch StreamDeck

## What This Is

Firmware for the **BigTreeTech PandaTouch** (ESP32-S3) that transforms the device into a BLE HID Stream Deck with a 5-inch touchscreen UI. Users configure up to 100 buttons across 5 pages, each executing keyboard shortcuts, media keys, app launchers, text macros, or Home Assistant HTTP actions. Configuration happens via the onboard touchscreen or a web dashboard over WiFi.

## Core Value

A plug-and-play BLE Stream Deck for PandaTouch owners that just works — reliable connection, responsive UI, and zero friction configuration via web or touch.

## Requirements

### Validated

- [x] BLE HID keyboard emulation (shortcuts, media keys, app launchers)
- [x] Multi-page button grid (1–5 pages × up to 20 buttons, configurable grid 2×2 to 5×3)
- [x] Web dashboard for remote configuration over WiFi
- [x] Multiple OS profiles (Windows / macOS / Linux) with separate button sets
- [x] Sleep mode with wake-on-touch (LVGL overlay, no action on wake touch)
- [x] OTA firmware updates via web dashboard
- [x] EN/ES localization
- [x] Configurable brightness via slider
- [x] Disabled button type (explicit no-op, visually grayed out)

### Active

- [ ] UI stability: settings menu scroll without triggering actions (button list on separate screen)
- [ ] Text macros: new button type that types full strings via BLE
- [ ] Home Assistant HTTP integration: new button type for GET/POST to HA endpoints
- [ ] Web dashboard complete: full support for all button types including text macros and HA
- [ ] Custom image support polished: web upload + reliable display on device

### Out of Scope

- MQTT broker support — HTTP REST is sufficient for HA integration without extra infrastructure
- Multi-device pairing — one device, one host at a time
- Cloud sync / mobile app — local-only, WiFi-based web dashboard is the UX
- Port to other ESP32 hardware — PandaTouch is the only target

## Context

- **Hardware**: BigTreeTech PandaTouch — ESP32-S3 @ 240MHz, 16MB flash, 8MB PSRAM, 800×480 5" RGB display, GT911 touch controller
- **Stack**: Arduino/PlatformIO (espressif32@7.0.0), LVGL 9.3.0, ESP32 BLE Keyboard (Bluedroid), ESPAsyncWebServer, LittleFS, ArduinoJson
- **Current version**: v1.8.0 — multi-page, sleep mode, disabled buttons, settings UI refactored
- **BLE note**: Uses `ESP_LE_AUTH_BOND` (no MITM) with automatic bond clearing on reconnect loop to prevent the GATT_INSUF_AUTHENTICATION disconnect cycle
- **Public release target**: Open-source for PandaTouch owners; AGENTS.md already documents the codebase for contributors

## Decisions

| Decision | Choice | Rationale |
|---|---|---|
| BLE stack | Bluedroid (not NimBLE) | espressif32@7.0.0 default; NimBLE env exists as future option |
| UI framework | LVGL 9.3.0 | Screen management via lv_scr_load, sleep via transparent overlay |
| HA integration | HTTP REST | No broker dependency, simpler UX |
| Storage format | Per-OS binary files (v2) + NVS for settings | Fast load, clear OS separation |
| Security | ESP_LE_AUTH_BOND | Sufficient for local HID device |
