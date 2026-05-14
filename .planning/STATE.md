# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-13)

**Core value:** A plug-and-play BLE Stream Deck for PandaTouch owners that just works — reliable connection, responsive UI, and zero friction configuration via web or touch.
**Current focus:** Phase 1 — UI Stability & Bug Fixes

## Current Position

Phase: 1 of 4 (UI Stability & Bug Fixes)
Plan: Not yet planned
Status: Ready to plan
Last activity: 2026-05-13 — Roadmap created; requirements, architecture, and codebase concerns reviewed

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 0
- Average duration: -
- Total execution time: -

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

*Updated after each plan completion*

## Accumulated Context

### Decisions

- Bluedroid BLE stack retained (not NimBLE) for v1.0 — NimBLE scaffolded as future option
- HTTP REST (not MQTT) for Home Assistant integration — no broker dependency
- Per-OS binary files (v2 format) in LittleFS + NVS for scalar settings
- No automated tests — hardware-only validation

### Pending Todos

None yet.

### Blockers / Concerns

- **Phase 1**: Sleep overlay orphan bug confirmed (streamdeck.cpp:24-53) — device never re-sleeps after settings screen timeout. Fix required before Phase 2.
- **Phase 1**: Double-tap crash on selection screens (ui_helpers.cpp:10-21) — fix is known, add guard before lv_obj_del.
- **Phase 3**: HTTP button must be non-blocking — delay() in button callback freezes LVGL. Must use async/FreeRTOS pattern.
- **All phases**: Backup/restore reads v1 file paths (CONCERNS.md) — known bug, deferred past v1.0.0 unless it blocks release.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Tech debt | Backup/restore v1 file path bug | Deferred post-v1.0 | Roadmap creation |
| Tech debt | pt_display.h inline functions / static per-TU variable | Deferred post-v1.0 | Roadmap creation |
| Feature | LVGL 9.3 → 9.5 upgrade | Deferred post-v1.0 | Roadmap creation |
| Feature | NimBLE migration | Deferred post-v1.0 | Roadmap creation |

## Session Continuity

Last session: 2026-05-13
Stopped at: Roadmap written; STATE.md initialized. No plans created yet.
Resume file: None
