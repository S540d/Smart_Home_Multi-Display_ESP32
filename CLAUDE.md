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
- **LDR Sensor**: Pin 34 (for ambient light detection)

## Code Architecture

### Core Structure

The codebase follows a modular architecture with clear separation of concerns:

- **main.cpp**: System initialization, main loop, and global object definitions
- **config.h**: Central configuration with namespaces for colors, layout, timing, and data structures
- **display.h/cpp**: Display rendering and UI components
- **network.h/cpp**: WiFi and MQTT connectivity
- **sensors.h/cpp**: Sensor data processing
- **system.h/cpp**: System utilities and helper functions
- **utils.h/cpp**: General utility functions

### Key Data Structures

- **SensorData**: Enhanced struct with change detection, trend analysis, and smart formatting
- **SystemStatus**: Comprehensive system metrics including memory, network, time, and hardware info
- **RenderManager**: Optimized rendering system to avoid unnecessary redraws
- **AntiBurninManager**: Prevents display burn-in with periodic pixel shifting

### Display System

The display uses an intelligent rendering system:
- Change detection prevents unnecessary redraws
- Anti-burn-in management shifts content periodically
- Modular UI components (sensor boxes, progress bars, indicators)
- RGB565 color scheme optimized for the ST7789 display

### Network Configuration

WiFi and MQTT settings are defined in config.h:
- MQTT topics are centrally managed in NetworkConfig::MqttTopics
- Automatic reconnection handling for both WiFi and MQTT
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