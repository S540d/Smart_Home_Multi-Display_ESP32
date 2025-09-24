# Smart Home Multi-Display ESP32

A WiFi-connected smart home display system built on ESP32 that shows real-time sensor data including renewable energy metrics, electricity prices, weather data, and more on a 320x240 TFT display.

## Features

### 🏠 **Smart Home Dashboard**
- **Real-time Data Display**: Shows comprehensive energy and environmental data:
  - 🌱 Renewable energy share percentage with status indicators
  - 💰 Current electricity price + Day-Ahead market prices
  - ⚡ PV generation with intelligent energy distribution visualization
  - 🔋 House battery and car charging levels
  - 🏠 Home consumption and wallbox power monitoring
  - 🌡️ Water and outdoor temperature readings
  - 📈 Stock prices with trend analysis and percentage changes

### 📱 **Interactive Touch Interface**
- **Touch Navigation**: Tap any sensor box to access detailed subpages
  - 🌱 **Renewable Energy Details**: Ökostrom analysis with 24h price charts
  - 💰 **Price Analysis**: Day-Ahead electricity prices with hourly breakdown
  - 🔋 **Charging Status**: Battery levels for house storage and car
  - ⚡ **Consumption Monitor**: Home usage and wallbox power details
  - ⚙️ **Settings**: Touch calibration and system information

### ⚡ **Advanced Energy Visualization**
- **Segmented Progress Bars**: PV energy distribution shows:
  - 🟢 Green: Power flowing to car/wallbox
  - 🔵 Blue: Power charging house battery
  - 🔴 Red: Power fed back to grid
- **Bidirectional Energy Flow**: Visual representation of grid import/export
- **Real-time Power Management**: Smart energy routing visualization

### 🔧 **System Features**
- **MQTT Integration**: Comprehensive IoT connectivity with 15+ topics
- **WiFi Connectivity**: Robust auto-reconnection with signal strength display
- **Anti-Burn-in Protection**: Intelligent pixel shifting to preserve display
- **OTA Updates**: Seamless over-the-air firmware updates
- **Memory Management**: Advanced heap monitoring with automatic cleanup
- **Performance Optimization**: Selective rendering and change detection

## Hardware Requirements

- **ESP32 Development Board JC2432W328**
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

   **Note**: If you skip this step, the build will fail with a clear error message explaining what to do.

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

#### **Core Sensor Data**
- `home/PV/Share_renewable` - Renewable energy percentage
- `home/PV/EnergyPrice` - Current electricity price
- `home/PV/EnergyPriceDayAhead` - Day-Ahead market prices (24h JSON data)
- `home/stocks/CL2PACurr` - Stock price (current)
- `home/stocks/CL2PARef` - Stock price (reference)
- `home/stocks/CL2PAPrevClose` - Stock price (previous close)
- `home/Weather/OutdoorTemperature` - Outdoor temperature
- `home/Heating/WaterTemperature` - Water temperature

#### **Power Management (all values in kW)**
- `home/PV/PVCurrentPower` - Current PV generation
- `home/PV/GridCurrentPower` - Grid power (import/export)
- `home/PV/LoadCurrentPower` - Total house consumption
- `home/PV/StorageCurrentPower` - Battery charging/discharging
- `home/PV/WallboxPower` - Electric car charging power
- `home/PV/chargingLevel` - House battery charge level (%)

### Display Layout

The 320x240 display features a sophisticated **3x3 grid layout**:

#### **Main Screen**
- **Row 1**: Market/Financial data (Renewable %, Price, Stock)
- **Row 2**: Power/Charge data (Battery %, Consumption, PV Generation)
- **Row 3**: Environmental data (Outdoor temp, Water temp, Settings)
- **Status Bar**: WiFi/MQTT indicators, current time
- **System Info**: RAM usage, CPU load, uptime, LDR sensor

#### **Interactive Elements**
- **Touch-sensitive sensor boxes** with color-coded status indicators
- **Progress bars** with intelligent scaling and color coding
- **Trend arrows** showing value changes over time
- **Anti-burn-in** content shifting every 15 minutes

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
