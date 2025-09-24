#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════════
//                           ZENTRALE NAMESPACES UND KONSTANTEN
// ═══════════════════════════════════════════════════════════════════════════════

namespace Colors {
  // RGB565 Farbdefinitionen - KORRIGIERT: Rot und Blau waren vertauscht
  constexpr uint16_t BG_MAIN = 0x0000;           // #000000 Schwarz
  constexpr uint16_t BG_BOX = 0xA800;            // #000088 Dunkelblau - R/B getauscht
  constexpr uint16_t BG_BOX_TIMEOUT = 0x0011;    // #880000 Dunkelrot für Timeouts - R/B getauscht
  constexpr uint16_t BG_CALENDAR = 0x0020;       // #000040 Kalender-Hintergrund
  
  // Reihen-spezifische Hintergrundfarben - Dezente Blautnöne (RGB565)
  constexpr uint16_t BG_ROW1 = 0x1044;           // Dezentes Dunkelblau - Markt/Finanzen (RGB: 16,8,32)  
  constexpr uint16_t BG_ROW2 = 0x1865;           // Dezentes Mittelblau - Power/Charge (RGB: 24,12,40)
  constexpr uint16_t BG_ROW3 = 0x2086;           // Dezentes Hellblau - Umwelt (RGB: 32,16,48)
  
  constexpr uint16_t BORDER_MAIN = 0xFFFF;       // #FFFFFF Weiß
  constexpr uint16_t BORDER_INDICATOR = 0x8C71;  // #888888 Grau für Ampel-Rahmen
  constexpr uint16_t BORDER_PROGRESS = 0xBDF7;   // #BBBBBB Hellgrau für Progress-Bar
  
  constexpr uint16_t TEXT_MAIN = 0xFFFF;         // #FFFFFF Weiß
  constexpr uint16_t TEXT_LABEL = 0xBDF7;        // #BBBBBB Hellgrau für Labels
  constexpr uint16_t TEXT_TIMEOUT = 0x8410;      // #808080 Grau für Timeout-Text
  constexpr uint16_t TEXT_CALENDAR = 0xFFE0;     // #FFFF00 Gelb für Kalender
  
  constexpr uint16_t STATUS_GREEN = 0x07E0;      // #00FF00 Echtes Grün (bleibt)
  constexpr uint16_t STATUS_RED = 0x001F;        // #FF0000 Echtes Rot (R/B getauscht)
  constexpr uint16_t STATUS_YELLOW = 0xFFE0;     // #FFFF00 Echtes Gelb (bleibt)  
  constexpr uint16_t STATUS_ORANGE = 0x051F;     // #FF6600 Orange (R/B getauscht)
  constexpr uint16_t STATUS_CYAN = 0xF800;       // #00FFFF Cyan (R/B getauscht)
  constexpr uint16_t STATUS_BLUE = 0x8000;       // #000080 Dunkelblau (R/B getauscht)
  
  constexpr uint16_t TREND_UP = 0x07E0;          // Grün für steigende Werte (bleibt)
  constexpr uint16_t TREND_UP_STRONG = 0x05E0;   // Hellgrün für stark steigende Werte
  constexpr uint16_t TREND_DOWN = 0x001F;        // Rot für fallende Werte (R/B getauscht)
  constexpr uint16_t TREND_DOWN_STRONG = 0x003F; // Hellrot für stark fallende Werte
  constexpr uint16_t TREND_NEUTRAL = 0xBDF7;     // Hellgrau für unveränderte Werte (wie Labels)
  
  constexpr uint16_t SYSTEM_OK = 0xF800;         // Cyan für OK-Status (R/B getauscht)
  constexpr uint16_t SYSTEM_WARN = 0x051F;       // Orange für Warnungen (R/B getauscht)
  constexpr uint16_t SYSTEM_ERROR = 0x001F;      // Rot für Fehler (R/B getauscht)

  // Touch-Visualisierung
  constexpr uint16_t TOUCH_MARKER = 0xF81F;      // Magenta für Touch-Markierung
  constexpr uint16_t TOUCH_BORDER = 0xFFFF;      // Weiß für Touch-Rahmen
}

namespace Layout {
  // Display-Layout Konstanten
  constexpr int DISPLAY_WIDTH = 320;
  constexpr int DISPLAY_HEIGHT = 240;
  
  // Sensor-Box Dimensionen
  constexpr int SENSOR_BOX_WIDTH = 100;
  constexpr int SENSOR_BOX_HEIGHT = 40;
  constexpr int SENSOR_BOX_WIDTH_WIDE = 190;
  constexpr int SENSOR_BOX_WIDTH_MEDIUM = 90;
  constexpr int SENSOR_BOX_RADIUS = 3;
  
  // UI-Element Größen
  constexpr int INDICATOR_RADIUS = 5;
  constexpr int PROGRESS_BAR_HEIGHT = 4;
  constexpr int SYSTEM_INFO_WIDTH = 80;
  constexpr int SYSTEM_INFO_HEIGHT = 60;  // Erweitert für Uhrzeit
  
  // Abstände und Offsets
  constexpr int PADDING_SMALL = 3;
  constexpr int PADDING_MEDIUM = 5;
  constexpr int PADDING_LARGE = 10;
  constexpr int LINE_SPACING = 12;

  // Position Konstanten
  constexpr int STATUS_BAR_Y = 10;
  constexpr int STATUS_BAR_WIDTH = 200;
  constexpr int STATUS_BAR_HEIGHT = 20;

  constexpr int SYSTEM_INFO_X = 240;
  constexpr int SYSTEM_INFO_Y = 200;
  constexpr int SYSTEM_INFO_EXTENDED_HEIGHT = 20;

  constexpr int NETWORK_INFO_X = 10;
  constexpr int NETWORK_INFO_Y = 200;
  constexpr int NETWORK_INFO_WIDTH = 120;
  constexpr int NETWORK_INFO_LINES = 3;

  constexpr int TIME_DISPLAY_X = 240;
  constexpr int TIME_DISPLAY_Y = 8;
  constexpr int TIME_DISPLAY_WIDTH = 80;
  constexpr int TIME_DISPLAY_HEIGHT = 32;

  // Touch und Chart Konstanten
  constexpr int MAX_TOUCH_MARKERS = 5;
  constexpr int CHART_HOURS = 24;
  constexpr int CHART_HOUR_INTERVAL = 4;

  // Home Screen Layout Positionen
  constexpr int TITLE_X = 10;
  constexpr int TITLE_Y = 10;
  constexpr int COMBINED_LAYOUT_X1 = 10;
  constexpr int COMBINED_LAYOUT_Y1 = 35;
  constexpr int COMBINED_LAYOUT_X2 = 115;
  constexpr int COMBINED_LAYOUT_Y2 = 135;
}

namespace Timing {
  // Timeout-Definitionen
  constexpr unsigned long SENSOR_TIMEOUT_MS = 300000;
  constexpr unsigned long WIFI_RECONNECT_INTERVAL = 30000;
  constexpr unsigned long MQTT_RECONNECT_INTERVAL = 5000;
  
  // Update-Intervalle
  constexpr unsigned long ANTI_BURNIN_INTERVAL = 900000; // 15 Minuten
  constexpr unsigned long SYSTEM_UPDATE_INTERVAL = 2000;
  constexpr unsigned long TIMEOUT_CHECK_INTERVAL = 10000;
  constexpr unsigned long RENDER_UPDATE_INTERVAL = 500;
  constexpr unsigned long TIME_UPDATE_INTERVAL = 60000;  // Uhrzeit alle Minute aktualisieren
  
  // Network Timeouts
  constexpr int WIFI_CONNECT_TIMEOUT_S = 30;
  constexpr int MQTT_CONNECT_TIMEOUT_MS = 10000;

  // Touch-Visualisierung
  constexpr unsigned long TOUCH_MARKER_DURATION_MS = 2000; // 2 Sekunden Touch-Markierung
}

namespace PowerManagement {
  // Power thresholds and limits
  constexpr float MIN_CONSUMPTION_THRESHOLD = 0.1f;  // Minimum power for calculations
  constexpr float MAX_PV_POWER = 30.0f;             // Maximum expected PV power
  constexpr float MAX_GRID_POWER = 50.0f;           // Maximum expected grid power
  constexpr float MAX_LOAD_POWER = 50.0f;           // Maximum expected load power
  constexpr float MAX_STORAGE_POWER = 20.0f;        // Maximum expected storage power
  constexpr float GRID_BALANCE_THRESHOLD = 0.1f;    // Grid near-zero threshold

  // Power data ages (milliseconds)
  constexpr unsigned long MAX_POWER_DATA_AGE_MS = 300000; // 5 minutes

  // Energy flow visualization
  constexpr int BIDIRECTIONAL_BAR_HEIGHT = 4;
  constexpr int POWER_BAR_MIN_WIDTH = 10;
  constexpr int POWER_DISPLAY_PRECISION = 1;  // Decimal places for power display
}

namespace System {
  // Memory Management
  constexpr uint32_t MIN_FREE_HEAP = 50000;
  constexpr uint32_t CRITICAL_HEAP_LEVEL = 25000;
  constexpr int MAX_STRING_LENGTH = 64;
  
  // Anti-Burnin
  constexpr int ANTI_BURNIN_MAX_OFFSET = 10;
  constexpr int ANTI_BURNIN_STEPS = 2;
  
  // Sensor-Konfiguration
  constexpr int SENSOR_COUNT = 8;  // Ohne Eco-Box (untere rechte Ecke entfernt)
  constexpr int CPU_SMOOTHING_SAMPLES = 5;
  
  // Pin-Definitionen
  constexpr int LDR_PIN = 34;
  
  // ADC-Test Pins für LDR-Stabilitätstest (ADC1 Pins 34-39)
  constexpr int ADC_TEST_PIN_34 = 34;
  constexpr int ADC_TEST_PIN_35 = 35;
  constexpr int ADC_TEST_PIN_36 = 36;
  constexpr int ADC_TEST_PIN_37 = 37;
  constexpr int ADC_TEST_PIN_38 = 38;
  constexpr int ADC_TEST_PIN_39 = 39;
  
  // Zeit-Konfiguration (als extern deklariert)
  extern const char* const NTP_SERVER;
  extern const char* const TIMEZONE;
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              NETZWERK-KONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

namespace NetworkConfig {
  extern const char* const WIFI_SSID;
  extern const char* const WIFI_PASSWORD;
  extern const char* const MQTT_SERVER;
  constexpr int MQTT_PORT = 1883;
  extern const char* const MQTT_USER;
  extern const char* const MQTT_PASS;
  
  struct MqttTopics {
    const char* const data[System::SENSOR_COUNT] = {
      "home/PV/Share_renewable",       // [0] Ökostrom (Reihe 1)
      "home/PV/EnergyPrice",           // [1] Preis (Reihe 1)
      "home/stocks/CL2PACurr",         // [2] Aktie (Reihe 1) - war [7]
      "home/PV/chargingLevel",         // [3] Ladestand (Reihe 2)
      "",                              // [4] PV/Netz (intern berechnet, kein MQTT)
      "home/PV/WallboxPower",          // [5] Wallbox (Reihe 2) - war [2]
      "home/Weather/OutdoorTemperature", // [6] Außen (Reihe 3)
      "home/Heating/WaterTemperature"   // [7] Wasser (Reihe 3) - war [5]
    };
    
    const char* const stockReference = "home/stocks/CL2PARef";
    const char* const stockPreviousClose = "home/stocks/CL2PAPrevClose";
    const char* const historyRequest = "display/history_request";
    const char* const historyResponse = "display/history_response";
    const char* const energyMarketPriceDayAhead = "EnergyMarketPriceDayAhead";
    
    // Power Management Topics
    const char* const pvPower = "home/PV/CurrentPower";          // PV-Erzeugung
    const char* const gridPower = "home/PV/GridCurrentPower";     // Netzbezug/Einspeisung
    const char* const loadPower = "home/PV/LoadCurrentPower";     // Hausverbrauch
    const char* const storagePower = "home/PV/StorageCurrentPower"; // Speicher Lade/Entladung
  };
  
  extern const MqttTopics topics;
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              DATENSTRUKTUREN
// ═══════════════════════════════════════════════════════════════════════════════

// Erweiterte Sensor-Datenstruktur mit Change-Detection
struct SensorData {
  // Wert-Management
  float value = 0.0f;
  float lastValue = 0.0f;
  float previousRenderValue = NAN;     // Für Change-Detection beim Rendering
  
  // Metadaten
  char label[16] = "";                 // Sichere String-Länge
  char unit[8] = "";                   // Einheit (%, W, °C, etc.)
  char formattedValue[16] = "";        // Formatierter Anzeige-Wert
  
  // Status-Tracking
  bool hasChanged = false;
  bool isTimedOut = true;              // Start mit Timeout
  bool requiresRedraw = true;          // Für selektive Redraws
  unsigned long lastUpdate = 0;
  
  // Layout-Informationen
  struct Layout {
    int x, y, w, h;
    bool hasProgressBar;
    bool hasIndicator;
    bool hasBidirectionalBar;
  } layout;
  
  // Trend-Analyse
  enum TrendDirection { STABLE, UP, DOWN } trend = STABLE;

  // Touch-Interaktion
  unsigned int touchCount = 0;         // Anzahl der Touch-Events
  unsigned long lastTouchTime = 0;     // Letzter Touch-Zeitstempel
  
  // Intelligente Wert-Formatierung
  void formatValue() {
    if (isTimedOut) {
      strcpy(formattedValue, "---");
      return;
    }
    
    // Spezielle Formatierung je nach Sensor-Typ
    if (strcmp(unit, "W") == 0 && value >= 1000) {
      snprintf(formattedValue, sizeof(formattedValue), "%.1fkW", value / 1000.0f);
    } else if (strcmp(unit, "€") == 0) {
      snprintf(formattedValue, sizeof(formattedValue), "%.2f%s", value, unit);
    } else {
      snprintf(formattedValue, sizeof(formattedValue), "%.1f%s", value, unit);
    }
  }
  
  // Change-Detection für optimierte Redraws
  bool needsRedraw() const {
    return requiresRedraw || 
           isnan(previousRenderValue) || 
           abs(value - previousRenderValue) > 0.01f ||
           hasChanged;
  }
  
  void markRendered() {
    previousRenderValue = value;
    requiresRedraw = false;
    hasChanged = false;
  }
};

// System-Status mit erweiterten Metriken
struct SystemStatus {
  // Netzwerk-Status
  bool wifiConnected = false;
  bool mqttConnected = false;
  int wifiRSSI = 0;
  unsigned long wifiReconnectAttempts = 0;
  unsigned long mqttReconnectAttempts = 0;
  
  // Performance-Metriken
  uint32_t freeHeap = 0;
  uint32_t minFreeHeap = UINT32_MAX;
  float cpuUsageSmoothed = 0.0f;
  float cpuSamples[System::CPU_SMOOTHING_SAMPLES] = {0};
  int cpuSampleIndex = 0;
  
  // Hardware-Info
  int ldrValue = 0;
  unsigned long uptime = 0;           // in Sekunden
  
  // Warnungen und Status
  bool lowMemoryWarning = false;
  bool criticalMemoryWarning = false;
  bool hasNetworkError = false;
  
  // Zeit-Management
  char currentTime[9] = "00:00:00";  // HH:MM:SS
  char currentDate[11] = "01.01.2024"; // DD.MM.YYYY
  bool timeValid = false;
  unsigned long lastTimeUpdate = 0;
  
  // Chip-Information (einmalig ermittelt)
  struct ChipInfo {
    uint8_t chipRevision = 0;
    uint32_t chipId = 0;
    bool hasPSRAM = false;
    size_t psramSize = 0;
    uint32_t flashSize = 0;
  } chipInfo;
  
  // Performance-Counter
  struct Performance {
    unsigned long totalRedraws = 0;
    unsigned long skippedRedraws = 0;
    float redrawEfficiency = 100.0f;   // Prozent eingesparte Redraws
  } performance;
  
  void updateCpuUsage(float newSample) {
    cpuSamples[cpuSampleIndex] = newSample;
    cpuSampleIndex = (cpuSampleIndex + 1) % System::CPU_SMOOTHING_SAMPLES;
    
    // Gleitender Durchschnitt berechnen
    float sum = 0;
    for (int i = 0; i < System::CPU_SMOOTHING_SAMPLES; i++) {
      sum += cpuSamples[i];
    }
    cpuUsageSmoothed = sum / System::CPU_SMOOTHING_SAMPLES;
  }
  
  void updateMemoryStatus() {
    freeHeap = ESP.getFreeHeap();
    if (freeHeap < minFreeHeap) {
      minFreeHeap = freeHeap;
    }
    
    lowMemoryWarning = (freeHeap < System::MIN_FREE_HEAP);
    criticalMemoryWarning = (freeHeap < System::CRITICAL_HEAP_LEVEL);
  }
  
  void updateTime() {
    unsigned long now = millis();
    if (now - lastTimeUpdate < Timing::TIME_UPDATE_INTERVAL && timeValid) {
      return; // Nicht zu oft aktualisieren
    }
    lastTimeUpdate = now;
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      strftime(currentTime, sizeof(currentTime), "%H:%M:%S", &timeinfo);
      strftime(currentDate, sizeof(currentDate), "%d.%m.%Y", &timeinfo);
      timeValid = true;
    } else {
      strcpy(currentTime, "--:--:--");
      strcpy(currentDate, "--.--.----");
      timeValid = false;
    }
  }
};

// Anti-Burnin Management - Verbessert für korrekte Löschung
struct AntiBurninManager {
  unsigned long lastShift = 0;
  int currentOffsetX = 0;
  int currentOffsetY = 0;
  int lastOffsetX = 0;  // Für Löschung der alten Position
  int lastOffsetY = 0;
  bool moveRight = true;
  bool isEnabled = true;
  bool offsetChanged = false;
  
  void update() {
    if (!isEnabled) return;
    
    unsigned long now = millis();
    if (now - lastShift >= Timing::ANTI_BURNIN_INTERVAL) {
      lastShift = now;
      
      // Alte Position speichern
      lastOffsetX = currentOffsetX;
      lastOffsetY = currentOffsetY;
      
      // Neue Position berechnen
      if (moveRight) {
        currentOffsetX = System::ANTI_BURNIN_MAX_OFFSET;
        moveRight = false;
        Serial.println("Anti-Burnin: →10px");
      } else {
        currentOffsetX = 0;
        moveRight = true;
        Serial.println("Anti-Burnin: ←0px");
      }
      
      offsetChanged = true;
    }
  }
  
  int getOffsetX() const { return currentOffsetX; }
  int getOffsetY() const { return currentOffsetY; }
  int getLastOffsetX() const { return lastOffsetX; }
  int getLastOffsetY() const { return lastOffsetY; }
  bool hasOffsetChanged() { 
    bool changed = offsetChanged;
    offsetChanged = false;
    return changed;
  }
};

// Render-Manager für effiziente Updates
struct RenderManager {
  bool fullRedrawRequired = true;
  unsigned long lastRenderUpdate = 0;
  bool systemInfoChanged = false;
  bool networkStatusChanged = false;
  bool timeChanged = false;
  
  struct ChangeFlags {
    bool sensors[System::SENSOR_COUNT] = {false};
    bool systemInfo = false;
    bool networkStatus = false;
    bool time = false;
    bool fullScreen = false;
    bool antiBurnin = false;
  } changes;
  
  void markSensorChanged(int index) {
    if (index >= 0 && index < System::SENSOR_COUNT) {
      changes.sensors[index] = true;
    }
  }
  
  void markSystemInfoChanged() { changes.systemInfo = true; }
  void markNetworkStatusChanged() { changes.networkStatus = true; }
  void markTimeChanged() { changes.time = true; }
  void markFullRedrawRequired() { changes.fullScreen = true; }
  void markAntiBurninChanged() { changes.antiBurnin = true; }
  
  void clearAllFlags() {
    memset(&changes, false, sizeof(changes));
    fullRedrawRequired = false;
  }
  
  bool needsUpdate() const {
    unsigned long now = millis();
    return (now - lastRenderUpdate >= Timing::RENDER_UPDATE_INTERVAL) &&
           (fullRedrawRequired || hasAnyChanges());
  }
  
  bool hasAnyChanges() const {
    if (changes.fullScreen || changes.systemInfo || 
        changes.networkStatus || changes.time || changes.antiBurnin) {
      return true;
    }
    
    for (int i = 0; i < System::SENSOR_COUNT; i++) {
      if (changes.sensors[i]) return true;
    }
    
    return false;
  }
};

// Display-Modi
enum DisplayMode { HOME_SCREEN, HISTORY_SCREEN, PRICE_DETAIL_SCREEN };

// ═══════════════════════════════════════════════════════════════════════════════
//                              PRICE DETAIL DATA STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════════

struct HourlyPrice {
  float price = 0.0f;
  char hour[6] = "";  // Format: "HH:MM"
  bool isValid = false;
};

struct DayAheadPriceData {
  HourlyPrice prices[24];  // 24 Stunden des Tages
  unsigned long lastUpdate = 0;
  bool hasData = false;
  char date[11] = "";  // Format: "DD.MM.YYYY"

  void clear() {
    for (int i = 0; i < 24; i++) {
      prices[i].isValid = false;
      prices[i].price = 0.0f;
      strcpy(prices[i].hour, "");
    }
    hasData = false;
    lastUpdate = 0;
    strcpy(date, "");
  }
};

#endif // CONFIG_H