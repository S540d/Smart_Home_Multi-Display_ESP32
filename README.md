# Smart Home Multi-Display ESP32

A small hobby project: a touch display for the wall that shows my smart home data (energy flow, electricity price, car charge level, temperatures, next calendar event) and raises full-screen alarms. It is a personal build, shared as-is for inspiration. There is no installation guide and no support.

## Hardware

- **Guition JC2432W328** – 2.8" 320x240 ESP32 display board (ST7789 TFT, CST820 capacitive touch, ambient light sensor). Board overview and specs: [atomic14 – Guition JC2432W328](https://www.atomic14.com/esp32/boards/guition-jc2432w328/). It is the capacitive variant of the "Cheap Yellow Display" (ESP32-2432S028) family.
- The display is configured in [`src/display_lgfx.h`](src/display_lgfx.h) ([LovyanGFX](https://github.com/lovyan03/LovyanGFX)).

Pins used on this board:

| Component | ESP32 pin |
|-----------|-----------|
| TFT MOSI / SCLK / CS / DC | 13 / 14 / 15 / 2 |
| TFT backlight | 27 |
| Touch SDA / SCL / INT / RST (CST820) | 33 / 32 / 21 / 25 |
| Light sensor (LDR) | 34 |

> GPIO 33/32 (touch I2C) sit next to the LDR input (GPIO 34, ADC1). Do not reuse them for other functions.

## What it does

- **Home screen** with five zones: time and status, electricity price and renewable share, energy flow (PV generation, consumption, origin bar), temperatures and storage / car charge level, next calendar event.
- **Detail pages** on tap: day-ahead price chart, charging status, wallbox monitor, device list (smart plugs), settings with touch calibration.
- **Full-screen alarms:** garage door open, ventilation needed (CO2 above threshold). A tap acknowledges them.
- **Anti-burn-in:** the content is shifted periodically; brightness follows the ambient light.
- **Data via MQTT** from a home server (Node-RED), including the car state of charge from [TeslaMate](https://github.com/teslamate-org/teslamate).
- **OTA updates** over Wi-Fi (PlatformIO environment `esp32dev_ota`).
- Built as FreeRTOS tasks (touch, network, display, system) around a single state object, with change detection to avoid unnecessary redraws.

## MQTT topics

Topic names are defined in [`src/config.h`](src/config.h); the values below are what my setup publishes.

| Topic | Content |
|-------|---------|
| `home/PV/Share_renewable` | Renewable share (%) |
| `home/PV/EnergyPrice` | Current electricity price |
| `home/energy/price_forecast_24h` | Day-ahead prices, JSON `[{"h":hour,"v":price}, ...]` |
| `home/PV/PVCurrentPower`, `GridCurrentPower`, `LoadCurrentPower`, `StorageCurrentPower`, `WallboxPower` | Power values (kW) |
| `home/PV/chargingLevel` | House battery level (%) |
| `home/Weather/OutdoorTemperature`, `home/Heating/WaterTemperature` | Temperatures |
| `teslamate/cars/1/battery_level`, `teslamate/cars/1/state` | Car state of charge and state (published by TeslaMate) |
| `home/display/calendar/next` | Next calendar event as plain text |
| `home/fritz/plug<N>/state`, `.../set` | Smart plug state / switch command |
| `house/garage/door/state` | `open` / `closed` (drives the garage alarm) |
| `house/rooms/wemos/sensors/air_quality/eCO2` | eCO2 in ppm (drives the ventilation alarm) |

## Configuration and secrets

Credentials (Wi-Fi, MQTT, OTA) live in `src/config_secrets.h`, which is git-ignored. [`config_template.h`](config_template.h) shows the expected names.

## License

[PolyForm Noncommercial License 1.0.0](LICENSE).
