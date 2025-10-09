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
  constexpr uint16_t STATUS_RED = 0xF800;        // Echtes Rot (war 0x001F, jetzt richtig)
  constexpr uint16_t STATUS_YELLOW = 0xFFE0;     // #FFFF00 Echtes Gelb (bleibt)
  constexpr uint16_t STATUS_ORANGE = 0xFD20;     // Orange (heller als vorher)
  constexpr uint16_t STATUS_CYAN = 0x07FF;       // Cyan (korrigiert)
  constexpr uint16_t STATUS_BLUE = 0x001F;       // Helles Blau (war 0x8000, jetzt gut sichtbar)
  
  constexpr uint16_t TREND_UP = 0x07E0;          // Grün für steigende Werte (bleibt)
  constexpr uint16_t TREND_UP_STRONG = 0x05E0;   // Hellgrün für stark steigende Werte
  constexpr uint16_t TREND_DOWN = 0xF800;        // Rot für fallende Werte (korrigiert)
  constexpr uint16_t TREND_DOWN_STRONG = 0xF000; // Hellrot für stark fallende Werte (korrigiert)
  constexpr uint16_t TREND_NEUTRAL = 0xBDF7;     // Hellgrau für unveränderte Werte (wie Labels)
  
  constexpr uint16_t SYSTEM_OK = 0x07FF;         // Cyan für OK-Status (korrigiert)
  constexpr uint16_t SYSTEM_WARN = 0xFD20;       // Orange für Warnungen (korrigiert)
  constexpr uint16_t SYSTEM_ERROR = 0xF800;      // Rot für Fehler (korrigiert)

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
  // Power thresholds and limits (in kW)
  constexpr float MIN_CONSUMPTION_THRESHOLD = 0.1f;  // Minimaler Leistungswert für Berechnungen - unter diesem Wert wird als "kein Verbrauch" gewertet
  constexpr float MAX_PV_POWER = 30.0f;             // Maximale erwartete PV-Anlagenleistung für Skalierung der Balkenanzeigen
  constexpr float MAX_GRID_POWER = 50.0f;           // Maximale erwartete Netzleistung (Bezug/Einspeisung) für Balken-Skalierung
  constexpr float MAX_LOAD_POWER = 50.0f;           // Maximaler erwarteter Hausverbrauch für Skalierung der Verbrauchsanzeige
  constexpr float MAX_STORAGE_POWER = 20.0f;        // Maximale Speicherleistung (Laden/Entladen) für Balken-Skalierung
  constexpr float GRID_BALANCE_THRESHOLD = 0.1f;    // Schwellenwert für "perfekte Energiebilanz" - Grid-Power unter diesem Wert zeigt Balance-Symbol

  // Power data ages (milliseconds)
  constexpr unsigned long MAX_POWER_DATA_AGE_MS = 300000; // Maximales Alter der Leistungsdaten (5 Min) bevor sie als veraltet gelten

  // Energy flow visualization (UI-Parameter)
  constexpr int BIDIRECTIONAL_BAR_HEIGHT = 4;       // Höhe der bidirektionalen Energiefluss-Balken in Pixeln
  constexpr int POWER_BAR_MIN_WIDTH = 10;           // Minimale Breite für Leistungsbalken-Segmente für bessere Sichtbarkeit
  constexpr int POWER_DISPLAY_PRECISION = 1;        // Dezimalstellen für Leistungsanzeigen (1 = eine Nachkommastelle)
}

namespace System {
  // Memory Management
  constexpr uint32_t MIN_FREE_HEAP = 50000;
  constexpr uint32_t CRITICAL_HEAP_LEVEL = 25000;
  constexpr int MAX_STRING_LENGTH = 512;  // Erhöht für Day-Ahead JSON Arrays
  
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
  
  // MQTT Topics als direkte Konstanten
  constexpr const char* const TOPIC_DATA[System::SENSOR_COUNT] = {
    "home/PV/Share_renewable",       // [0] Ökostrom (Reihe 1)
    "home/PV/EnergyPrice",           // [1] Preis (Reihe 1)
    "home/stocks/CL2PACurr",         // [2] Aktie (Reihe 1) - war [7]
    "home/PV/chargingLevel",         // [3] Ladestand des Hausspeichers (Reihe 2)
    "",                              // [4] PV/Netz (intern berechnet, kein MQTT)
    "home/PV/PVCurrentPower",          // [5] PV-Erzeugung (war Wallbox)
    "home/Weather/OutdoorTemperature", // [6] Außen (Reihe 3)
    "home/Heating/WaterTemperature"   // [7] Wasser (Reihe 3) - war [5]
  };

  constexpr const char* const STOCK_REFERENCE = "home/stocks/CL2PARef";
  constexpr const char* const STOCK_PREVIOUS_CLOSE = "home/stocks/CL2PAPrevClose";
  constexpr const char* const HISTORY_REQUEST = "display/history_request";
  constexpr const char* const HISTORY_RESPONSE = "display/history_response";
  constexpr const char* const ENERGY_MARKET_PRICE_DAY_AHEAD = "home/energy/price_forecast_24h";

  // Power Management Topics (alle Werte in kW als Float)
  constexpr const char* const PV_POWER = "home/PV/PVCurrentPower";          // Aktuelle PV-Erzeugungsleistung (immer positiv)
  constexpr const char* const GRID_POWER = "home/PV/GridCurrentPower";     // Aktuelle Netzleistung (Betrag - Richtung wird berechnet)
  constexpr const char* const LOAD_POWER = "home/PV/LoadCurrentPower";     // Aktueller Hausverbrauch (immer positiv)
  constexpr const char* const STORAGE_POWER = "home/PV/StorageCurrentPower"; // Aktuelle Speicherleistung (Betrag - Richtung wird berechnet)
  constexpr const char* const WALLBOX_POWER = "home/PV/WallboxPower";      // Aktuelle Wallbox-Leistung (immer positiv)
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
    } else if (strcmp(unit, "ct") == 0) {
      snprintf(formattedValue, sizeof(formattedValue), "%.1f%s", value, unit);
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
  int ldrValueSmoothed = 0;  // Geglätteter LDR-Wert für stabile Anzeige
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

// Anti-Burnin Management - Verbessert mit ±2 Pixel in alle Richtungen
struct AntiBurninManager {
  unsigned long lastShift = 0;
  int currentOffsetX = 0;
  int currentOffsetY = 0;
  int lastOffsetX = 0;  // Für Löschung der alten Position
  int lastOffsetY = 0;
  int shiftState = 0;  // 0=center, 1=right, 2=left, 3=down, 4=up
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

      // Neue Position berechnen - rotiere durch 5 Zustände
      shiftState = (shiftState + 1) % 5;

      switch (shiftState) {
        case 0:  // Center
          currentOffsetX = 0;
          currentOffsetY = 0;
          Serial.println("Anti-Burnin: ⊙ Center");
          break;
        case 1:  // Right +2px
          currentOffsetX = 2;
          currentOffsetY = 0;
          Serial.println("Anti-Burnin: → +2px");
          break;
        case 2:  // Left -2px
          currentOffsetX = -2;
          currentOffsetY = 0;
          Serial.println("Anti-Burnin: ← -2px");
          break;
        case 3:  // Down +2px
          currentOffsetX = 0;
          currentOffsetY = 2;
          Serial.println("Anti-Burnin: ↓ +2px");
          break;
        case 4:  // Up -2px
          currentOffsetX = 0;
          currentOffsetY = -2;
          Serial.println("Anti-Burnin: ↑ -2px");
          break;
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
  
  // Safe coordinate application with bounds checking
  int applyOffsetX(int x, int width = 0) const {
    int result = x + currentOffsetX;
    // Ensure the element stays within display bounds
    if (width > 0) {
      // For elements with width, ensure right edge doesn't exceed display
      result = constrain(result, 0, Layout::DISPLAY_WIDTH - width);
    } else {
      // For single points, ensure within display width
      result = constrain(result, 0, Layout::DISPLAY_WIDTH - 1);
    }
    return result;
  }
  
  int applyOffsetY(int y, int height = 0) const {
    int result = y + currentOffsetY;
    // Ensure the element stays within display bounds
    if (height > 0) {
      // For elements with height, ensure bottom edge doesn't exceed display
      result = constrain(result, 0, Layout::DISPLAY_HEIGHT - height);
    } else {
      // For single points, ensure within display height
      result = constrain(result, 0, Layout::DISPLAY_HEIGHT - 1);
    }
    return result;
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
enum DisplayMode {
  HOME_SCREEN,
  HISTORY_SCREEN,
  PRICE_DETAIL_SCREEN,
  OEKOSTROM_DETAIL_SCREEN,
  WALLBOX_CONSUMPTION_SCREEN,
  LADESTAND_SCREEN,
  DAYAHEAD_SCREEN,
  SETTINGS_SCREEN
};

// ═══════════════════════════════════════════════════════════════════════════════
//                              PRICE DETAIL DATA STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════════

enum PriceCategory {
  PRICE_VERY_CHEAP = 0,    // < 20% percentile
  PRICE_CHEAP = 1,         // 20-40% percentile
  PRICE_MEDIUM = 2,        // 40-60% percentile
  PRICE_EXPENSIVE = 3,     // 60-80% percentile
  PRICE_VERY_EXPENSIVE = 4 // > 80% percentile
};

enum TrendDirection {
  TREND_RISING = 0,
  TREND_FALLING = 1,
  TREND_STABLE = 2
};

struct OptimalUsageWindow {
  uint8_t startHour;      // 0-23
  uint8_t endHour;        // 0-23
  float averagePrice;     // Average price in this window
  float savingsVsPeak;    // Potential savings vs most expensive hour
  bool isAvailable;       // Whether this window is valid
};

struct HourlyPrice {
  float price = 0.0f;
  char hour[6] = "";      // Format: "HH:MM"
  bool isValid = false;
  PriceCategory category = PRICE_MEDIUM;
};

struct EnhancedDayAheadData {
  // Raw price data (existing)
  HourlyPrice prices[24];  // 24 Stunden des Tages
  unsigned long lastUpdate = 0;
  bool hasData = false;
  char date[11] = "";  // Format: "DD.MM.YYYY"

  // Calculated analytics
  float dailyAverage = 0.0f;
  float minPrice = 0.0f;
  float maxPrice = 0.0f;
  uint8_t cheapestHour = 0;     // 0-23, hour with lowest price
  uint8_t expensiveHour = 0;    // 0-23, hour with highest price

  // Trend analysis
  TrendDirection trend = TREND_STABLE;
  float volatilityIndex = 0.0f; // 0-100, price volatility measure

  // Optimization insights
  OptimalUsageWindow optimalWindows[3]; // Best 3 time windows for high consumption
  float potentialSavings = 0.0f;        // Max savings possible by timing usage optimally

  // Data quality metrics
  uint8_t dataQuality = 0;              // 0-100, confidence in the data
  unsigned long lastAnalysis = 0;       // When analytics were last calculated

  void clear() {
    for (int i = 0; i < 24; i++) {
      prices[i].isValid = false;
      prices[i].price = 0.0f;
      prices[i].category = PRICE_MEDIUM;
      strcpy(prices[i].hour, "");
    }
    hasData = false;
    lastUpdate = 0;
    lastAnalysis = 0;
    strcpy(date, "");

    // Reset analytics
    dailyAverage = 0.0f;
    minPrice = 0.0f;
    maxPrice = 0.0f;
    cheapestHour = 0;
    expensiveHour = 0;
    trend = TREND_STABLE;
    volatilityIndex = 0.0f;
    potentialSavings = 0.0f;
    dataQuality = 0;

    // Reset optimal windows
    for (int i = 0; i < 3; i++) {
      optimalWindows[i].isAvailable = false;
      optimalWindows[i].startHour = 0;
      optimalWindows[i].endHour = 0;
      optimalWindows[i].averagePrice = 0.0f;
      optimalWindows[i].savingsVsPeak = 0.0f;
    }
  }

  // Analytics calculation function
  void calculateAnalytics() {
    if (!hasData) return;

    // Count valid prices
    int validCount = 0;
    float sum = 0.0f;
    minPrice = 999.0f;
    maxPrice = 0.0f;

    for (int i = 0; i < 24; i++) {
      if (prices[i].isValid) {
        validCount++;
        sum += prices[i].price;

        if (prices[i].price < minPrice) {
          minPrice = prices[i].price;
          cheapestHour = i;
        }
        if (prices[i].price > maxPrice) {
          maxPrice = prices[i].price;
          expensiveHour = i;
        }
      }
    }

    if (validCount == 0) {
      dataQuality = 0;
      return;
    }

    // Calculate average
    dailyAverage = sum / validCount;

    // Calculate data quality (percentage of valid hours)
    dataQuality = (validCount * 100) / 24;

    // Calculate volatility (standard deviation as percentage of mean)
    if (validCount > 1 && dailyAverage > 0) {
      float variance = 0.0f;
      for (int i = 0; i < 24; i++) {
        if (prices[i].isValid) {
          float diff = prices[i].price - dailyAverage;
          variance += diff * diff;
        }
      }
      variance /= (validCount - 1);
      volatilityIndex = (sqrt(variance) / dailyAverage) * 100.0f;
    }

    // Categorize prices by percentiles
    categorizePrices();

    // Calculate trend direction (compare first 8h vs last 8h average)
    calculateTrend();

    // Find optimal usage windows
    findOptimalWindows();

    // Calculate potential savings
    potentialSavings = maxPrice - minPrice;

    lastAnalysis = millis();
  }

private:
  void categorizePrices() {
    if (!hasData) return;

    // Simple percentile-based categorization
    float range = maxPrice - minPrice;
    if (range <= 0) return;

    for (int i = 0; i < 24; i++) {
      if (prices[i].isValid) {
        float priceRatio = (prices[i].price - minPrice) / range;

        if (priceRatio <= 0.2f) {
          prices[i].category = PRICE_VERY_CHEAP;
        } else if (priceRatio <= 0.4f) {
          prices[i].category = PRICE_CHEAP;
        } else if (priceRatio <= 0.6f) {
          prices[i].category = PRICE_MEDIUM;
        } else if (priceRatio <= 0.8f) {
          prices[i].category = PRICE_EXPENSIVE;
        } else {
          prices[i].category = PRICE_VERY_EXPENSIVE;
        }
      }
    }
  }

  void calculateTrend() {
    // Compare first 8 hours vs last 8 hours average
    float firstHalfSum = 0.0f, lastHalfSum = 0.0f;
    int firstCount = 0, lastCount = 0;

    for (int i = 0; i < 8; i++) {
      if (prices[i].isValid) {
        firstHalfSum += prices[i].price;
        firstCount++;
      }
    }

    for (int i = 16; i < 24; i++) {
      if (prices[i].isValid) {
        lastHalfSum += prices[i].price;
        lastCount++;
      }
    }

    if (firstCount > 0 && lastCount > 0) {
      float firstAvg = firstHalfSum / firstCount;
      float lastAvg = lastHalfSum / lastCount;
      float change = ((lastAvg - firstAvg) / firstAvg) * 100.0f;

      if (change > 5.0f) {
        trend = TREND_RISING;
      } else if (change < -5.0f) {
        trend = TREND_FALLING;
      } else {
        trend = TREND_STABLE;
      }
    }
  }

  void findOptimalWindows() {
    // Find 3 best 3-hour windows for high consumption
    for (int w = 0; w < 3; w++) {
      optimalWindows[w].isAvailable = false;
      float bestAverage = 999.0f;
      int bestStart = 0;

      // Try all possible 3-hour windows
      for (int start = 0; start <= 21; start++) {
        float windowSum = 0.0f;
        int validInWindow = 0;
        bool skipWindow = false;

        // Check if this window overlaps with already selected ones
        for (int prev = 0; prev < w; prev++) {
          if (optimalWindows[prev].isAvailable) {
            if (!(start + 2 < optimalWindows[prev].startHour ||
                  start > optimalWindows[prev].endHour)) {
              skipWindow = true;
              break;
            }
          }
        }

        if (skipWindow) continue;

        // Calculate average for this 3-hour window
        for (int h = start; h < start + 3 && h < 24; h++) {
          if (prices[h].isValid) {
            windowSum += prices[h].price;
            validInWindow++;
          }
        }

        if (validInWindow >= 2) { // Require at least 2 valid hours
          float windowAverage = windowSum / validInWindow;
          if (windowAverage < bestAverage) {
            bestAverage = windowAverage;
            bestStart = start;
          }
        }
      }

      // Store the best window found
      if (bestAverage < 999.0f) {
        optimalWindows[w].isAvailable = true;
        optimalWindows[w].startHour = bestStart;
        optimalWindows[w].endHour = min(bestStart + 2, 23);
        optimalWindows[w].averagePrice = bestAverage;
        optimalWindows[w].savingsVsPeak = maxPrice - bestAverage;
      }
    }
  }
};

// Legacy typedef for backward compatibility
typedef EnhancedDayAheadData DayAheadPriceData;

#endif // CONFIG_H