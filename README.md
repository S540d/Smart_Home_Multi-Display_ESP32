# Smart Home Multi-Display ESP32

A WiFi-connected smart home display system built on ESP32 that shows real-time sensor data including renewable energy metrics, electricity prices, weather data, and more on a 320x240 TFT display.

## Features

- **Real-time Data Display**: Shows multiple sensor readings including:
  - Renewable energy share percentage
  - Current electricity price
  - Wallbox power consumption
  - Car charging level
  - Water temperature
  - Outdoor temperature
  - Stock prices with trend indicators
- **MQTT Integration**: Receives data via MQTT protocol
- **WiFi Connectivity**: Automatic connection and reconnection handling
- **Anti-Burn-in Protection**: Prevents display burn-in with content shifting
- **OTA Updates**: Over-the-air firmware updates
- **Ambient Light Detection**: LDR sensor for display brightness adjustment
- **Memory Management**: Continuous monitoring with automatic cleanup
- **Modular Architecture**: Clean, maintainable code structure

## Hardware Requirements

- **ESP32 Development Board**
- **ST7789 320x240 TFT Display**
- **LDR (Light Dependent Resistor)** for ambient light detection
- **Resistors and wiring** as per schematic

### Pin Configuration

| Component | ESP32 Pin |
|-----------|-----------|
| TFT MOSI  | 13        |
| TFT SCLK  | 14        |
| TFT CS    | 15        |
| TFT DC    | 2         |
| TFT RST   | Not connected |
| TFT BL    | 27        |
| LDR       | 34        |

## Software Requirements

- [PlatformIO](https://platformio.org/) IDE or CLI
- ESP32 Arduino Framework

## Installation

1. **Clone the repository**
   ```bash
   git clone <repository-url>
   cd Multidisplay_ESP32_Rev2
   ```

2. **Configure your credentials**
   ```bash
   cp config_template.h src/config_secrets.h
   ```

   Edit `src/config_secrets.h` with your actual:
   - WiFi SSID and password
   - MQTT server IP address
   - MQTT credentials (if required)
   - OTA update password

3. **Build and upload**
   ```bash
   pio run --target upload
   ```

4. **Monitor serial output** (optional)
   ```bash
   pio run --target monitor
   ```

## Configuration

### MQTT Topics

The system subscribes to the following MQTT topics (configured in `config.h`):

- `home/energy/renewable_share` - Renewable energy percentage
- `home/energy/electricity_price` - Current electricity price
- `home/wallbox/power` - Wallbox power consumption
- `home/car/charging_level` - Car battery level
- `home/sensors/water_temp` - Water temperature
- `home/sensors/outdoor_temp` - Outdoor temperature
- `home/finance/stock_price` - Stock price data

### Display Layout

The 320x240 display shows:
- **Top row**: WiFi and MQTT status indicators
- **Sensor grid**: 6 sensor value boxes with progress bars
- **Bottom area**: System information and time
- **Visual indicators**: Color-coded status and trend arrows

## Architecture

### Core Components

- **main.cpp**: System initialization and main loop
- **config.h**: Central configuration with namespaced constants
- **display.h/cpp**: TFT display rendering and UI components
- **network.h/cpp**: WiFi and MQTT connectivity management
- **sensors.h/cpp**: Sensor data processing and validation
- **utils.h/cpp**: Utility functions and helpers
- **system.h/cpp**: System status and memory management
- **ota.h/cpp**: Over-the-air update functionality

### Key Features

- **Change Detection**: Prevents unnecessary screen redraws
- **Memory Monitoring**: Continuous RAM usage tracking
- **Error Handling**: Comprehensive error detection and recovery
- **Modular Design**: Clean separation of concerns
- **Performance Optimization**: Efficient rendering and data processing

## Development

### Build Commands

```bash
# Build project
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio run --target monitor

# Clean build files
pio run --target clean

# Build and upload with monitoring
pio run --target upload --target monitor
```

### Code Style

- Namespaced constants for configuration
- Comprehensive error handling with try-catch blocks
- Memory-efficient string handling
- Modular, maintainable architecture
- Detailed serial logging for debugging

## Troubleshooting

### Common Issues

1. **WiFi Connection Failed**
   - Check SSID and password in `config_secrets.h`
   - Verify WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)

2. **MQTT Connection Issues**
   - Verify MQTT server IP address
   - Check if MQTT broker is running and accessible
   - Ensure firewall allows MQTT traffic (port 1883)

3. **Display Issues**
   - Check TFT wiring connections
   - Verify pin assignments match your hardware
   - Ensure adequate power supply (3.3V/5V)

4. **Memory Warnings**
   - Normal operation should show >50KB free RAM
   - Consider reducing sensor update frequency if persistent

### Debug Information

Enable serial monitoring to view:
- System startup information
- WiFi connection status
- MQTT message reception
- Memory usage statistics
- Error messages and warnings

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## Acknowledgments

- Built with [PlatformIO](https://platformio.org/)
- Uses [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) library for display
- MQTT communication via [PubSubClient](https://github.com/knolleary/pubsubclient)
- JSON parsing with [ArduinoJson](https://arduinojson.org/)