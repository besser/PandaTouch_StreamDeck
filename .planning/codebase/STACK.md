# Technology Stack

**Analysis Date:** 2026-05-13

## Languages

**Primary:**
- C++17 — All firmware logic: UI, BLE, storage, webserver (`src/*.cpp`, `src/*.h`, `src/pt/*.h`)

**Secondary:**
- C — ESP-IDF HAL calls (LEDC, BLE GAP), used inline within C++ files

## Runtime

**Environment:**
- ESP32-S3 (Xtensa LX7 dual-core, 240 MHz) — set explicitly via `setCpuFrequencyMhz(240)` in `src/main.cpp`
- PSRAM: QSPI OPI (Octal SPI) — enabled via `board_build.arduino.memory_type = qio_opi` and `-DBOARD_HAS_PSRAM`
- FreeRTOS — provided by ESP-IDF; LVGL uses it for the draw thread (`LV_USE_OS = LV_OS_FREERTOS`)

**Package Manager:**
- PlatformIO — `platformio.ini` at project root
- Lockfile: Not present (library versions pinned by version specifiers in `platformio.ini`)

## Frameworks

**Core:**
- Arduino framework (`framework = arduino`) via `espressif32@7.0.0` platform
- Arduino-ESP32 v2.x (implied by platform 7.0.0; a v3.x alternate env `pandatouch-arduino-3x` exists but is not the default)

**UI:**
- LVGL 9.3.0 — Touchscreen UI rendering, widget layout, input device abstraction
  - Config: `include/lv_conf.h`
  - OS integration: FreeRTOS (`LV_USE_OS = LV_OS_FREERTOS`)
  - Color depth: RGB565 16-bit (`LV_COLOR_DEPTH 16`)
  - Render mode: Partial double-buffer from PSRAM (`PT_LVGL_RENDER_PARTIAL_2_PSRAM`)
  - Draw thread stack: 8 KB, priority HIGH
  - Image cache: 128 KB (`LV_CACHE_DEF_SIZE`)
  - Image decoders enabled: LODEPNG (PNG), TJPGD (JPEG)
  - Widgets enabled: Button, Label, Arc, Bar, Dropdown, Image, Keyboard, List, Slider, Spinner, Textarea, Flex layout, Grid layout
  - Theme: Default (light mode, grow-on-press, 80 ms transitions)
  - Fonts: Montserrat 12, 14, 18, 24; default is `lv_font_montserrat_14`
  - LittleFS integration: `LV_USE_FS_ARDUINO_ESP_LITTLEFS = 1`, driver letter `'L'`

**Build/Dev:**
- PlatformIO CLI — build, flash, monitor
- Monitor baud rate: 115200

## Key Dependencies

**Critical:**
- `lvgl/lvgl@9.3.0` — UI framework; all screen rendering and touch input
- `tamctec/TAMC_GT911@1.0.2` — GT911 capacitive touch controller driver (I2C); `src/pt/pt_display.h`
- `moononournation/GFX Library for Arduino@1.5.0` — RGB565 panel driver (`Arduino_ESP32RGBPanel`, `Arduino_RGB_Display`); `src/pt/pt_display.h`
- `T-vK/ESP32 BLE Keyboard@^0.3.2` — BLE HID keyboard emulation; `src/ble_actions.cpp`
- `bblanchon/ArduinoJson@^7.0.0` — JSON serialize/deserialize for web API; `src/webserver.cpp`
- `ESPAsyncWebServer` (GitHub: me-no-dev) — Async HTTP server on port 80; `src/webserver.cpp`
- `AsyncTCP` (GitHub: me-no-dev) — Dependency of ESPAsyncWebServer

**Infrastructure:**
- `WiFi` (Arduino built-in) — Station mode WiFi; `src/storage.cpp`, `src/webserver.cpp`
- `LittleFS` (Arduino built-in) — Filesystem for button configs and image assets; `src/storage.cpp`
- `ArduinoOTA` (Arduino built-in) — OTA updates over WiFi; `src/streamdeck.cpp`
- `Preferences` (Arduino built-in / NVS) — Key-value settings in NVS flash; `src/storage.cpp`

## Configuration

**Environment:**
- No `.env` file; all runtime config stored in NVS via `Preferences` namespace `"deck"`
- WiFi SSID/password stored in NVS keys `"wssid"` / `"wpass"`
- No secrets file; credentials entered through web dashboard

**Build:**
- `platformio.ini` — board, platform, lib_deps, build_flags
- `include/lv_conf.h` — LVGL feature configuration (1384 lines)
- `src/pt/pt_board.h` — Hardware pin definitions (LCD RGB, GT911 touch, backlight)
- `partitions_custom.csv` — Custom flash partition table

**Compiler Flags:**
- `-I include` — exposes `include/lv_conf.h` to LVGL
- `-DLV_CONF_INCLUDE_SIMPLE` — LVGL finds config via `lv_conf.h` filename directly
- `-DBOARD_HAS_PSRAM` — enables PSRAM heap caps in Arduino framework
- `-DARDUINO_LOOP_STACK_SIZE=16384` — expands Arduino loop task stack to 16 KB

## Flash / Memory Layout

**Flash size:** 16 MB (`board_upload.flash_size = 16MB`)
**Flash mode:** QIO at 80 MHz (`board_build.flash_mode = qio`, `board_build.f_flash = 80000000L`)

**Partition table** (`partitions_custom.csv`):
| Partition | Type | Size |
|-----------|------|------|
| nvs | data/nvs | 20 KB (0x5000) |
| otadata | data/ota | 8 KB (0x2000) |
| app0 (ota_0) | app | 3 MB |
| app1 (ota_1) | app | 3 MB |
| spiffs | data/spiffs | ~10 MB (0x9D0000) |

Note: The "spiffs" label is used but the firmware mounts it as LittleFS — this is valid; Arduino LittleFS reads SPIFFS-typed partitions.

## Platform Requirements

**Development:**
- PlatformIO Core or IDE extension
- Python (PlatformIO dependency)
- USB connection to ESP32-S3 for initial flash; subsequent updates via ArduinoOTA or web `/api/update`

**Production:**
- ESP32-S3 module with 16 MB flash and PSRAM
- 800x480 RGB LCD panel wired per `src/pt/pt_board.h` pin definitions
- GT911 touch controller on I2C0 (SCL=GPIO1, SDA=GPIO2)
- Backlight on GPIO21 via LEDC PWM (30 kHz, 11-bit resolution)
- GPIO46 for LCD reset

---

*Stack analysis: 2026-05-13*
