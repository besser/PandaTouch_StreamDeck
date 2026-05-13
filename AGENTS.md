# 🐼 PandaTouch StreamDeck - Developer & Agent Guide

This document serves as a technical map for developers and AI agents working on this project.

## 🏗 Project Architecture

The project is built for the **BigTreeTech PandaTouch** (ESP32-S3) using the **Arduino framework** and **PlatformIO**. It transforms the device into a Bluetooth HID (Human Interface Device) that emulates a keyboard.

### Core Components

- **`StreamDeckApp` (`src/streamdeck.cpp`)**: The central orchestrator. It manages the lifecycle (setup/loop) and coordinates between BLE, UI, and Web services.
- **`pt_display` (`src/pt/pt_display.cpp`)**: Hardware abstraction layer for the PandaTouch. Handles the 5-inch RGB screen (ST7262 or similar) and GT911 touch controller via LVGL.
- **`ble_actions` (`src/ble_actions.cpp`)**: Manages the `BleKeyboard` instance. Implements logic for Windows/macOS shortcuts, media keys, and localized keyboard layouts (US/ES).
- **`ui_main` (`src/ui_main.cpp`)**: Implements the LVGL-based touchscreen interface. Displays the button grid and handles touch events.
- **`storage` (`src/storage.cpp`)**: Persistent configuration management using **LittleFS** and **ArduinoJson**. Stores WiFi credentials, button mappings, and system settings in `/config.json`.
- **`webserver` (`src/webserver.cpp`)**: An asynchronous web dashboard (`ESPAsyncWebServer`) for remote configuration, icon management, and OTA updates.

## 🛠 Tech Stack

- **Framework**: Arduino / ESP-IDF (v5.1 via PlatformIO espressif32@7.0.0).
- **UI Library**: LVGL 9.3.0.
- **HID**: ESP32 BLE Keyboard (HID over GATT).
- **Storage**: LittleFS.
- **Networking**: WiFi, ESPAsyncWebServer, ArduinoOTA.

## 📂 File Structure

- `src/main.cpp`: Entry point, initializes hardware and starts the app.
- `src/constants.h`: Global definitions, pinouts, and default settings.
- `src/pt/`: PandaTouch hardware-specific drivers.
- `src/ui_*.cpp`: UI screens (Main, Settings).
- `src/l10n.*`: Localization support.

## 🤖 Guidelines for AI Agents

### 1. Hardware Awareness
- The PandaTouch has **PSRAM** enabled (`-DBOARD_HAS_PSRAM`). Use it for large LVGL buffers.
- Flash size is **16MB**. The partition table (`partitions_custom.csv`) allocates significant space for LittleFS.
- Screen resolution is **800x480**.

### 2. UI Development
- Use LVGL 9.x APIs. Note that many tutorials online use LVGL 8.x; ensure you use the correct syntax for v9 (e.g., `lv_display_t` instead of `lv_disp_t`).
- UI changes should be requested via `g_pending_ui_update = true` if called from outside the main loop context (though mostly everything runs in the same thread here).

### 3. Storage & JSON
- Always check if LittleFS is mounted before access.
- Configuration is stored in `/config.json`. Use `ArduinoJson` for parsing/serialization.
- Be mindful of memory when parsing large JSON files; use `JsonDocument`.

### 4. BLE Actions
- Shortcut logic distinguishes between `OS_WINDOWS`, `OS_MACOS`, and `OS_LINUX`.
- `OS_LINUX` uses `Alt+F2` for application launching by default.
- To add a new key type, update `handle_button_action` in `ble_actions.cpp`.

### 6. Visual Feedback
- Buttons have a visual "pressed" state implemented via `LV_STATE_PRESSED` style in `src/ui_main.cpp`. This provides tactile feedback on the touchscreen.

### 5. Webserver
- The HTML/JS for the web dashboard is stored as a C-string in `webserver_html.h` to simplify deployment, though it can also serve files from LittleFS.

## 🚀 Common Tasks

- **Adding a new button type**:
  1. Update `ButtonType` enum in `constants.h`.
  2. Implement logic in `ble_actions.cpp`.
  3. Update the Web Dashboard JS in `webserver_html.h` to support the new type.
- **Modifying hardware pins**:
  - Check `src/pt/pt_board.h` and `src/constants.h`.

## ⚠️ Known Limitations
- **BLE/WiFi Coexistence**: Using both simultaneously on ESP32 can lead to performance drops. The project minimizes WiFi usage once the initial config is done.
- **OTA**: v1.6.0+ fixed critical OTA bugs. Ensure the partition table is correctly flashed via USB for the first time.
