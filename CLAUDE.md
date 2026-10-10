# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a PlatformIO ESP32 project for a smart home multi-display system. The device connects to WiFi and MQTT to receive sensor data (renewable energy share, electricity price, wallbox power, car charging level, water temperature, outdoor temperature, and stock price) and displays them on a 320x240 ST7789 TFT display.

## Build Commands

```bash
# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio run --target monitor

# Clean build files
pio run --target clean

# Build and upload in one command
pio run --target upload --target monitor
```

## Hardware Configuration

- **Board**: ESP32 Dev Module
- **Display**: ST7789 320x240 TFT
  - MOSI: Pin 13
  - SCLK: Pin 14  
  - CS: Pin 15
  - DC: Pin 2
  - RST: Not connected (-1)
  - BL (Backlight): Pin 27
- **Touch (CST820, I2C)**:
  - SDA: Pin 33
  - SCL: Pin 32
  - INT: Pin 21
  - RST: Pin 25
- **LDR Sensor**: Pin 34 (for ambient light detection; ADC1_CH6 — directly adjacent to the touch I2C pins 32/33, see Issue #1)

**Pin conflict warning**: GPIO 33 is the CST820 I2C SDA line. Never assign it to anything else (e.g. TFT_eSPI `TOUCH_CS`) — this caused the LDR instability in Issue #1.

## Code Architecture

### Core Structure

The codebase follows a modular architecture with clear separation of concerns:

- **main.cpp**: System initialization, hardware object definitions (`tft`, `client`, `espClient`); `loop()` sleeps via `vTaskDelay(portMAX_DELAY)` — all work is done in FreeRTOS tasks
- **state.h**: `SystemState` singleton — the single source of truth for all application state (see below)
- **tasks.h/cpp**: FreeRTOS task implementations (`touchTask`, `networkTask`, `displayTask`, `systemTask`) and inter-task queues
- **config.h**: Central configuration with namespaces for colors, layout, timing, and all data-structure type definitions
- **display.h/cpp**: Display rendering and UI components
- **network.h/cpp**: WiFi and MQTT connectivity
- **sensors.h/cpp**: Sensor data processing
- **utils.h/cpp**: General utility functions

### State Management

All application state lives in a single `SystemState` singleton (`src/state.h`):

```cpp
// Access pattern used everywhere:
static SystemState& appState = SystemState::getInstance();
appState.pvPower = 3.5f;
appState.sensors[0].value = 42.0f;
```

- **Do not add new global variables to `main.cpp`** — add fields to `SystemState` instead
- Hardware handles (`tft`, `client`, `espClient`, `touchManager`) remain as regular globals — they are not state
- `SystemState::begin()` must be called early in `setup()` to initialize the recursive FreeRTOS mutex
- All tasks protect shared state with `xSemaphoreTakeRecursive(appState.stateMutex, ...)` / `xSemaphoreGiveRecursive(appState.stateMutex)`
- `updateDisplay()` reads state **without** the mutex — brief stale display data is an accepted trade-off

### Key Data Structures

- **SystemState** (`state.h`): Singleton holding all runtime state — power values, sensor data, display mode, timing, SmartPlug states, etc.
- **SensorData** (`config.h`): Enhanced struct with change detection, trend analysis, and smart formatting
- **SystemStatus** (`config.h`): Comprehensive system metrics including memory, network, time, and hardware info
- **RenderManager** (`config.h`): Optimized rendering system to avoid unnecessary redraws
- **AntiBurninManager** (`config.h`): Prevents display burn-in with periodic pixel shifting
- **LastValidPowerValues** (`config.h`): Fallback power readings for MQTT outages

### Display System

The display uses an intelligent rendering system:
- Change detection prevents unnecessary redraws
- Anti-burn-in management shifts content periodically
- Modular UI components (sensor boxes, progress bars, indicators)
- RGB565 color scheme optimized for the ST7789 display
- **Display driver**: LovyanGFX (migrated from TFT_eSPI in Issue #22, see below). Device
  config lives in `src/display_lgfx.h` (`class LGFX`); pins/SPI params are unchanged
  from the old TFT_eSPI build flags. `tft` is a global `LGFX` instance, same usage
  pattern as before (`tft.drawString(...)`, `tft.fillRect(...)`, etc.) — only basic
  primitives are used, no sprites/DMA/pushImage.

### FreeRTOS Task Architecture

All runtime work runs in four pinned tasks (Issue #23, merged PR #51):

| Task | Core | Priority | Responsibility |
|------|------|----------|----------------|
| `touchTask` | 1 | 4 | 50 Hz touch polling → `touchEventQueue` |
| `networkTask` | 0 | 3 | WiFi / MQTT / OTA — sole owner of `client` |
| `displayTask` | 1 | 2 | Drain touch queue, anti-burnin, TFT render |
| `systemTask` | 0 | 1 | Memory, sensor timeouts, CPU scaling |

- **`touchEventQueue`** (size 10): `touchTask` → `displayTask`
- **`plugCommandQueue`** (size 5): `displayTask` → `networkTask` (for SmartPlug MQTT publish)
- TFT (`tft`) is used exclusively on Core 1 (touchTask + displayTask) — no SPI cross-core conflict
- WDT timeout: 60 s; each task self-registers with `esp_task_wdt_add(NULL)`

### Network Configuration

WiFi and MQTT settings are defined in config.h:
- MQTT topics are centrally managed in NetworkConfig::MqttTopics
- WiFi/MQTT reconnect handled exclusively in `networkTask`
- Sensor timeout detection (5 minutes default)

### Memory Management

- Continuous memory monitoring with warnings at <50KB free
- Critical memory handling at <25KB free
- Automatic cleanup routines for low memory situations

## Development Notes

- The system uses namespaced constants extensively for maintainability
- All strings are size-constrained to prevent memory issues
- The main loop includes comprehensive error handling with try-catch blocks
- Serial output provides detailed system information and debugging
- Hardware info is detected automatically at startup
- **Issue #27 completed**: All state centralized in `SystemState` singleton (merged PR #50)
- **Issue #23 completed**: FreeRTOS task-based architecture with four pinned tasks (merged PR #51)
- **Issue #1 completed**: Fixed GPIO 33 pin conflict (TFT_eSPI `TOUCH_CS` vs. CST820 I2C SDA) causing
  unstable LDR readings; hardened `readADCFiltered()` with `analogReadMilliVolts()` + EMA/deadband
  (merged PR #52)
- **Issue #22 in progress (PR #53, open, not yet merged)**: Display driver migration TFT_eSPI →
  LovyanGFX. LVGL was deliberately rejected (see Issue #44's library evaluation: board is
  ESP32-WROOM/JC2432W328 with **no PSRAM**, and LVGL's buffer/heap needs don't fit next to
  WiFi/MQTT on a system already near its heap warning thresholds). LovyanGFX is a ~90%
  API-compatible drop-in — no UI rewrite, existing `RenderManager`/`AntiBurninManager` untouched.
  **Still needed before merge** (build was blocked in the CI sandbox by network policy — 403 on the
  PlatformIO/Espressif registry, not a code issue): a local `pio run` build, then on-device
  verification — visual comparison against the old TFT_eSPI rendering, font slots 1/2/4 (GLCD/font2/
  font4) look correct, touch (CST820) and OTA/calibration screens still work, heap stays stable,
  and check the boot log's PSRAM size (confirms whether the board really has none). Follow-up ideas
  once this is stable and merged: LovyanGFX DMA/sprite rendering, LDR-driven backlight dimming
  (synergizes with Issue #1's now-stable LDR readings) — not part of PR #53.