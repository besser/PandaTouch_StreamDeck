# Testing Patterns

**Analysis Date:** 2026-05-13

## No Automated Test Framework

This is embedded firmware for an ESP32-S3 SoC driving a physical 800×480 RGB LCD, BLE HID keyboard, and capacitive touch panel. Automated unit testing is not used for the following reasons:

- **Hardware dependencies are pervasive.** LVGL requires a running display driver with real framebuffers allocated from PSRAM. BLE requires the ESP32 radio stack. LittleFS requires flash. There is no abstraction layer that could be mocked without rewriting the firmware.
- **The Arduino framework's `setup()`/`loop()` model** does not separate business logic from hardware I/O in a way that allows isolated unit testing of logic.
- **Binary size constraints.** A test harness large enough to stub LVGL, BLE, and the file system would consume more flash than the firmware itself.
- **The test target IS the device.** Flash via USB, observe via serial monitor and physical interaction.

There is no `test/` directory, no `jest.config.*`, no `vitest.config.*`, no `unity` or `googletest` dependency in `platformio.ini`.

## Build Environment

```bash
# Build and upload to connected device
pio run -e pandatouch --target upload

# Build only (no device required)
pio run -e pandatouch

# Open serial monitor (115200 baud)
pio device monitor --baud 115200

# Build + upload + monitor in one command
pio run -e pandatouch --target upload && pio device monitor --baud 115200
```

The monitor baud rate is set in `platformio.ini`:
```ini
monitor_speed = 115200
```

## Serial Debug Output — Primary Verification Tool

All subsystems emit structured `Serial.println`/`Serial.printf` output. The serial monitor (115200 baud) is the primary tool for verifying correct operation.

**Startup sequence to verify:**
```
=== PandaTouch StreamDeck Starting ===
LittleFS Mounted Successfully.
StreamDeckApp::setup() - Starting BLE initialization
BLE Keyboard initialized. Waiting for connection...
```

If LittleFS fails:
```
LittleFS Mount Failed
```

**Storage and migration events:**
```
STORAGE: Migrating /win_btns.bin to /win_btns_v2.bin...
STORAGE: Migration V2 successful.
STORAGE: size mismatch, resetting
```

**WiFi events:**
```
WiFi Connected! IP: 192.168.x.x
WiFi Disconnected.
```

**BLE connection events:**
```
*** BLE CONNECTED! ***
*** BLE DISCONNECTED ***
Still advertising... Waiting for connection.
BLE bonds cleared (reconnect loop detected)
```

**Web API events:**
```
WEB API: Configuration saved successfully
RESTORE: request handler called
RESTORE: JSON parsed OK
RESTORE: bg=#121212
RESTORE: rows=3
RESTORE: Writing /win_btns_v2.bin with 20 buttons
RESTORE: /win_btns_v2.bin written OK (3200 bytes)
```

**OTA events:**
```
OTA: Start updating sketch
OTA: Update Complete
OTA Error[0]: Auth Failed
OTA Error[1]: Begin Failed
```

## Manual Test Procedures

### 1. Boot and Display Test

**What to verify:** LCD initializes, UI renders, touch responds.

1. Flash firmware via USB (`pio run -e pandatouch --target upload`)
2. Open serial monitor — confirm startup sequence
3. Confirm main grid appears on the 800×480 display with correct button count (default 3×3)
4. Tap any button — confirm it depresses visually (press state style changes background)
5. Tap and hold the brightness slider in the bottom bar — confirm display brightness changes in real-time

**Pass criteria:** No `LittleFS Mount Failed`, grid visible, touch responds within 1 second of boot.

### 2. BLE Keyboard Test

**What to verify:** BLE advertises, pairs, and sends keystrokes.

1. Enable Bluetooth on a host PC or Mac
2. Scan for devices — "PandaTouch Deck" should appear
3. Pair (no PIN required — device uses `ESP_IO_CAP_NONE`)
4. Serial monitor should show `*** BLE CONNECTED! ***`
5. Configure a button as `BTN_TYPE_BASIC_COMBO` with value `c` (copy shortcut)
6. Tap the button — host should receive Ctrl+C (Windows/Linux) or Cmd+C (macOS)
7. Configure a button as `BTN_TYPE_APP` with value `notepad` — tap it — Notepad should open (Windows)
8. Configure a button as `BTN_TYPE_MEDIA` with value `volup` — tap it — volume should increase

**BLE bonding reset test:**
If the device shows a reconnect loop (`*** BLE CONNECTED! ***` immediately followed by `*** BLE DISCONNECTED ***` three or more times), the firmware auto-clears bonds. Confirm serial shows `BLE bonds cleared (reconnect loop detected)` and the host can re-pair cleanly.

### 3. Settings UI Test

**What to verify:** Settings screens navigate correctly, settings persist across reboot.

1. Tap "Config" button (bottom right of main screen)
2. Verify settings list appears with 8 items: Global Background Color, Grid Layout Size, Target OS, WiFi Setup, Keyboard Language, Pages, Sleep Timeout, Button Configuration
3. Tap "Grid Layout Size" → select "4×3" → confirm main screen rebuilds with 12 buttons
4. Tap "Config" → tap "Back" → confirm return to main screen without crash
5. Power cycle the device — confirm 4×3 grid persists (NVS read on boot)

**Settings rebuild flag test:**
Change keyboard language (EN→ES) — settings screen should rebuild on next open (all labels in Spanish). Change back to EN — labels return to English.

### 4. WiFi and Web Interface Test

**What to verify:** WiFi connects, web UI is reachable, config save works.

1. Navigate to Settings → WiFi Setup
2. Enter SSID and password, tap "Save & Connect"
3. Serial monitor should show `WiFi Connected! IP: 192.168.x.x`
4. The IP address should update in the bottom bar of the main screen
5. Open a browser to `http://<ip-address>/` — web dashboard should load
6. Modify a button label in the web UI, click Save
7. Serial monitor should show `WEB API: Configuration saved successfully`
8. Device display should update within one `loop()` cycle (next `g_pending_ui_update` check)

**WiFi persistence test:**
Power cycle the device — WiFi should reconnect automatically (credentials stored in NVS under keys `"wssid"` and `"wpass"`).

### 5. OTA Firmware Update Test

**What to verify:** OTA update completes without bricking device.

There are two OTA paths:

**Path A — ArduinoOTA (network, PlatformIO):**
1. Device must be WiFi-connected (IP visible on screen)
2. In `platformio.ini`, add the device IP to `upload_port`
3. Run `pio run -e pandatouch --target upload`
4. Serial monitor should show `OTA: Start updating sketch` then `OTA: Update Complete`
5. Device restarts; confirm new firmware version in startup banner

**Path B — Web UI OTA (.bin file upload):**
1. Build firmware: `pio run -e pandatouch` → produces `.pio/build/pandatouch/firmware.bin`
2. Navigate to `http://<device-ip>/` → find "Firmware OTA" section
3. Select the `.bin` file, click "Update"
4. Device display switches to the OTA static screen ("Updating... Please wait, do not power off")
5. LVGL task handler is suspended (`pt_display_suspended = true`), display buffers freed
6. After update completes, device restarts automatically
7. Confirm firmware version in serial output after reboot

**OTA static screen test:**
Verify that when OTA is triggered from the web UI, the LVGL render loop stops (no LVGL crashes) and the GFX library draws the plain text overlay directly to the panel.

### 6. Sleep Timeout Test

**What to verify:** Screen dims after inactivity, wakes on touch.

1. Navigate to Settings → Sleep Timeout → select "30 sec"
2. Do not touch the screen for 30 seconds
3. Backlight should dim to 0% (display off)
4. Tap the screen — first tap should wake display (transparent overlay intercepts it)
5. Subsequent taps should reach buttons normally
6. Serial monitor does not show explicit sleep messages — verify purely by physical observation

### 7. Multi-Page Navigation Test

**What to verify:** Page navigation works, pages persist across reboot.

1. Navigate to Settings → Pages → select "3"
2. Main screen should show "Page 1 / 3" in the bottom nav bar with left/right arrows
3. Tap the right arrow — "Page 2 / 3" should appear with that page's buttons
4. Configure a button on Page 2 that differs from Page 1
5. Power cycle — confirm current page is restored from NVS and the correct button layout loads

### 8. Backup and Restore Test

**What to verify:** JSON backup export and import round-trips correctly.

1. Configure several buttons with labels, types, and colors
2. Navigate to `http://<device-ip>/` → Backup & Restore → "Download Backup"
3. Save the JSON file
4. Reset all buttons to defaults (configure a blank button)
5. Use "Restore Backup" to upload the saved JSON
6. Serial monitor should show verbose `RESTORE:` lines for each field
7. Confirm buttons are restored to the previously configured values

## Regression Risk Areas

**After any change to `ButtonConfig` struct size or field order (`src/storage.h`):**
The binary files `/win_btns_v2.bin`, `/mac_btns_v2.bin`, `/linux_btns_v2.bin` store the struct as raw bytes. A struct size change will cause `load_settings()` to detect a size mismatch (`"STORAGE: size mismatch, resetting"`) and wipe all button configs. This is the intended recovery path, but it must be verified manually after such changes. Bump file version and add a migration lambda in `src/storage.cpp` matching the `migrate_v1_to_v2` pattern.

**After any change to LVGL callback registration or screen object pointers:**
Test the full navigation path: main → settings → edit button → save → back → settings → back → main. Each transition must not crash or leave dangling `lv_obj_t*` pointers. Pay attention to whether `lv_obj_del` or `lv_obj_del_async` is appropriate at each site (see CONVENTIONS.md LVGL Object Lifecycle Patterns).

**After BLE library changes or OTA updates:**
Old bonding keys may be stale. Manually unpair the device from the host's Bluetooth settings and re-pair, or trigger the rapid-disconnect auto-clear by reconnecting and immediately disconnecting three times.

---

*Testing analysis: 2026-05-13*
