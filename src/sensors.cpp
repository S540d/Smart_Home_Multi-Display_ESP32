#include "sensors.h"
static SystemState& appState = SystemState::getInstance();


// ═══════════════════════════════════════════════════════════════════════════════
//                              GLOBALE SENSOR-STATISTIKEN
// ═══════════════════════════════════════════════════════════════════════════════

SensorPerformance sensorStats[System::SENSOR_COUNT];

// ═══════════════════════════════════════════════════════════════════════════════
//                              SENSOR-TIMEOUT-MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void checkSensorTimeouts() {
  unsigned long now = millis();
  bool hasTimeoutChanges = false;
  int timeoutCount = 0;
  
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    bool wasTimedOut = appState.sensors[i].isTimedOut;
    bool isNowTimedOut = (now - appState.sensors[i].lastUpdate) > Timing::SENSOR_TIMEOUT_MS;
    
    if (wasTimedOut != isNowTimedOut) {
      appState.sensors[i].isTimedOut = isNowTimedOut;
      appState.sensors[i].hasChanged = true;
      hasTimeoutChanges = true;
      
      // Performance-Statistiken aktualisieren
      if (isNowTimedOut) {
        sensorStats[i].timeoutEvents++;
      }
      
      Serial.printf("%s Sensor %d (%s) %s\n", 
                   isNowTimedOut ? "TIMEOUT" : "OK",
                   i, appState.sensors[i].label, 
                   isNowTimedOut ? "TIMEOUT" : "wieder online");
      
      appState.renderManager.markSensorChanged(i);
    }
    
    if (appState.sensors[i].isTimedOut) {
      timeoutCount++;
    }
  }
  
  // Auto-/Kalender-Stale-Zustand: wird nur beim Zeichnen ausgewertet, daher hier
  // periodisch auf Wechsel pruefen und gezielt neu zeichnen lassen.
  bool carStale = appState.car.isStale();
  if (carStale != appState.car.staleShown) {
    appState.car.staleShown = carStale;
    appState.renderManager.markSensorChanged(3);  // PKW-Zeile in der Speicher-Box
  }
  bool calStale = appState.calendar.isStale();
  if (calStale != appState.calendar.staleShown) {
    appState.calendar.staleShown = calStale;
    appState.renderManager.markSystemInfoChanged();  // Zone E
  }

  if (hasTimeoutChanges) {
    Serial.printf("Timeout-Status: %d/%d Sensoren offline\n", timeoutCount, System::SENSOR_COUNT);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              SENSOR-STATISTIKEN UND ANALYSE
// ═══════════════════════════════════════════════════════════════════════════════

void logSensorStatus() {
  Serial.println("\nSENSOR-STATUS-ÜBERSICHT:");
  Serial.println("┌────┬─────────────┬──────────┬─────────────┬────────────┐");
  Serial.println("│ ID │ Name        │ Wert     │ Status      │ Letztes Update │");
  Serial.println("├────┼─────────────┼──────────┼─────────────┼────────────┤");
  
  unsigned long now = millis();
  
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    const SensorData& sensor = appState.sensors[i];
    
    String status = sensor.isTimedOut ? "TIMEOUT" : "ONLINE";
    String lastUpdate = sensor.isTimedOut ? "---" : 
                       String((now - sensor.lastUpdate) / 1000) + "s";
    
    Serial.printf("│ %2d │ %-11s │ %-8s │ %-11s │ %-10s │\n",
                 i, sensor.label, sensor.formattedValue, 
                 status.c_str(), lastUpdate.c_str());
  }
  
  Serial.println("└────┴─────────────┴──────────┴─────────────┴────────────┘");
  
  int onlineCount = getOnlineSensorCount();
  int timeoutCount = getTimeoutSensorCount();
  float reliability = (float)onlineCount / System::SENSOR_COUNT * 100.0f;
  
  Serial.printf("📈 Verfügbarkeit: %d/%d online (%.1f%%)\n", 
               onlineCount, System::SENSOR_COUNT, reliability);
}

int getOnlineSensorCount() {
  int count = 0;
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    if (!appState.sensors[i].isTimedOut) {
      count++;
    }
  }
  return count;
}

int getTimeoutSensorCount() {
  return System::SENSOR_COUNT - getOnlineSensorCount();
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              SENSOR-LAYOUT INITIALISIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

void initializeSensorLayouts() {
  Serial.println("Initialisiere Sensor-Layouts (Phase-3 Redesign)...");

  struct SensorLayout {
    const char* label;
    const char* unit;
    int x, y, w, h;
    bool hasProgressBar;
    bool hasIndicator;
    bool hasBidirectionalBar;
  };

  // Phase-3 Layout: 5-Zonen-Design (320x240px, Landscape)
  // Zone A (y=0..27):   Header – Zeit + Netzstatus
  // Zone B (y=28..73):  3 gleiche Felder: Preis | Ökostrom | Drucker-Button (je ~104px)
  // Zone C (y=74..135): Energie-Flow-Zone – PV [5] intern integriert
  // Zone D (y=136..199):Außentemp | Wassertemp | Speicher % (63px, 50% höher als ursprünglich)
  // Zone E (y=200..239):System-Info
  const SensorLayout layouts[System::SENSOR_COUNT] = {
    // [0] Ökostrom %  – Zone B Mitte       (108,28) 104x46
    {"Oekostrom",    "%",   108, 28,  104, 46, false, false, false},
    // [1] Preis ct    – Zone B links        (2,28)   104x46
    {"Preis",        "ct",    2, 28,  104, 46, false, false, false},
    // [2] Aktie EUR   – kein eigener Frame
    {"Aktie",        "EUR",   0,  0,    0,  0, false, false, false},
    // [3] Speicher %  – Zone D rechts, kombinierte Box (160,136) 158x63
    {"Speicher",     "%",   160, 136, 158, 63, true,  false, false},
    // [4] Verbrauch kW– Zone C integriert  (2,74) 316x62
    {"Verbrauch",    "kW",    2, 74,  316, 62, false, false, false},
    // [5] PV-Erzeugung– Zone C integriert (kein eigener Rahmen)
    {"PV-Erzeugung", "W",     0,  0,    0,  0, false, false, false},
    // [6] Außentemp °C– Zone D links, obere Hälfte der Temp-Box (2,136) 156x31
    {"Aussen",       "C",     2, 136, 156, 31, false, false, false},
    // [7] Wassertemp °C– Zone D links, untere Hälfte der Temp-Box (2,167) 156x32
    {"Wasser",       "C",     2, 167, 156, 32, false, true,  false},
  };
  
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    const auto& layout = layouts[i];
    auto& sensor = appState.sensors[i];

    strncpy(sensor.label, layout.label, sizeof(sensor.label) - 1);
    strncpy(sensor.unit, layout.unit, sizeof(sensor.unit) - 1);

    // lastUpdate=0 würde als "gerade eben" gelten (now-0 < TIMEOUT) → Sensor fälschlich online
    // Wert so setzen dass Timeout sofort gilt:
    sensor.lastUpdate = millis() - Timing::SENSOR_TIMEOUT_MS - 1;
    sensor.isTimedOut = true;

    sensor.layout.x = layout.x;
    sensor.layout.y = layout.y;
    sensor.layout.w = layout.w;
    sensor.layout.h = layout.h;
    sensor.layout.hasProgressBar = layout.hasProgressBar;
    sensor.layout.hasIndicator = layout.hasIndicator;
    sensor.layout.hasBidirectionalBar = layout.hasBidirectionalBar;
    
    sensor.requiresRedraw = true;
    
    // Performance-Statistiken initialisieren
    sensorStats[i].totalUpdates = 0;
    sensorStats[i].timeoutEvents = 0;
    sensorStats[i].averageUpdateInterval = 0.0f;
    sensorStats[i].lastPerformanceCheck = millis();
    
    Serial.printf("   [%d] %s (%d,%d %dx%d) ProgressBar=%s Indicator=%s BidirBar=%s\n", 
                 i, sensor.label, sensor.layout.x, sensor.layout.y, 
                 sensor.layout.w, sensor.layout.h,
                 sensor.layout.hasProgressBar ? "JA" : "NEIN",
                 sensor.layout.hasIndicator ? "JA" : "NEIN",
                 sensor.layout.hasBidirectionalBar ? "JA" : "NEIN");
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              SENSOR-DATENVALIDIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

bool isValidSensorValue(int index, float value) {
  if (index < 0 || index >= System::SENSOR_COUNT) {
    return false;
  }
  
  // NaN und Infinity prüfen
  if (isnan(value) || isinf(value)) {
    return false;
  }
  
  // Sensor-spezifische Wertebereiche
  switch (index) {
    case 0: // Ökostrom %
      return value >= 0.0f && value <= 100.0f;

    case 1: // Strompreis ct/kWh (Day-Ahead kann negativ oder >100 sein)
      return value >= -50.0f && value <= 200.0f;

    case 2: // Aktienkurs €
      return value >= 0.0f && value <= 10000.0f;

    case 3: // Ladestand %
      return value >= 0.0f && value <= 100.0f;

    case 4: // Verbrauch kW (intern berechnet)
      return value >= 0.0f && value <= 50.0f;

    case 5: // PV-Erzeugung kW
      return value >= 0.0f && value <= 30.0f;

    case 6: // Außentemperatur °C
      return value >= -50.0f && value <= 70.0f;

    case 7: // Wassertemperatur °C
      return value >= 0.0f && value <= 100.0f;

    default:
      return true;
  }
}

void validateSensorRange(int index, float& value) {
  if (!isValidSensorValue(index, value)) {
    Serial.printf("WARNUNG - Sensor %d: Ungültiger Wert %.2f erkannt\n", index, value);
    
    // Sensor-spezifische Korrektur
    switch (index) {
      case 0: case 3: // Prozent-Werte
        value = constrain(value, 0.0f, 100.0f);
        break;

      case 1: // Strompreis ct/kWh
        value = constrain(value, -50.0f, 200.0f);
        break;

      case 2: // Aktienkurs €
        value = constrain(value, 0.0f, 10000.0f);
        break;

      case 4: // Verbrauch kW
        value = constrain(value, 0.0f, 50.0f);
        break;

      case 5: // PV-Erzeugung kW
        value = constrain(value, 0.0f, 30.0f);
        break;

      case 6: // Außentemperatur °C
        value = constrain(value, -50.0f, 70.0f);
        break;

      case 7: // Wassertemperatur °C
        value = constrain(value, 0.0f, 100.0f);
        break;
    }
    
    Serial.printf("   → Korrigiert auf: %.2f\n", value);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              PERFORMANCE-ANALYSE
// ═══════════════════════════════════════════════════════════════════════════════

void updateSensorPerformance() {
  unsigned long now = millis();
  
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    SensorPerformance& stats = sensorStats[i];
    
    if (!appState.sensors[i].isTimedOut) {
      stats.totalUpdates++;
      
      // Update-Intervall berechnen (nur wenn es vorherige Updates gab)
      if (stats.lastPerformanceCheck > 0) {
        unsigned long interval = now - stats.lastPerformanceCheck;
        
        // Gleitender Durchschnitt für Update-Intervall
        if (stats.averageUpdateInterval == 0.0f) {
          stats.averageUpdateInterval = interval;
        } else {
          stats.averageUpdateInterval = (stats.averageUpdateInterval * 0.9f) + (interval * 0.1f);
        }
      }
      
      stats.lastPerformanceCheck = now;
    }
  }
}

void logSensorPerformance() {
  Serial.println("\n📈 SENSOR-PERFORMANCE-ANALYSE:");
  Serial.println("┌────┬─────────────┬─────────┬─────────┬──────────────┐");
  Serial.println("│ ID │ Name        │ Updates │ Timeout │ Avg Interval │");
  Serial.println("├────┼─────────────┼─────────┼─────────┼──────────────┤");
  
  unsigned long totalUpdates = 0;
  unsigned long totalTimeouts = 0;
  
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    const SensorPerformance& stats = sensorStats[i];
    
    String avgInterval = (stats.averageUpdateInterval > 0) ? 
                        String(stats.averageUpdateInterval / 1000.0f, 1) + "s" : "---";
    
    Serial.printf("│ %2d │ %-11s │ %7lu │ %7lu │ %12s │\n",
                 i, appState.sensors[i].label, stats.totalUpdates, 
                 stats.timeoutEvents, avgInterval.c_str());
    
    totalUpdates += stats.totalUpdates;
    totalTimeouts += stats.timeoutEvents;
  }
  
  Serial.println("└────┴─────────────┴─────────┴─────────┴──────────────┘");
  
  // Gesamtstatistik
  float systemUptime = (millis() - appState.systemStartTime) / 1000.0f;
  float updateRate = totalUpdates / systemUptime;
  float reliabilityRate = (totalUpdates > 0) ? 
                         (1.0f - (float)totalTimeouts / totalUpdates) * 100.0f : 0.0f;
  
  Serial.printf("Gesamt: %lu Updates, %.1f Updates/s, %.1f%% Zuverlässigkeit\n",
               totalUpdates, updateRate, reliabilityRate);
}