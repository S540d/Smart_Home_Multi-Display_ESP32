#include "sensors.h"

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
    bool wasTimedOut = sensors[i].isTimedOut;
    bool isNowTimedOut = (now - sensors[i].lastUpdate) > Timing::SENSOR_TIMEOUT_MS;
    
    if (wasTimedOut != isNowTimedOut) {
      sensors[i].isTimedOut = isNowTimedOut;
      sensors[i].hasChanged = true;
      hasTimeoutChanges = true;
      
      // Performance-Statistiken aktualisieren
      if (isNowTimedOut) {
        sensorStats[i].timeoutEvents++;
      }
      
      Serial.printf("%s Sensor %d (%s) %s\n", 
                   isNowTimedOut ? "TIMEOUT" : "OK",
                   i, sensors[i].label, 
                   isNowTimedOut ? "TIMEOUT" : "wieder online");
      
      renderManager.markSensorChanged(i);
    }
    
    if (sensors[i].isTimedOut) {
      timeoutCount++;
    }
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
    const SensorData& sensor = sensors[i];
    
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
    if (!sensors[i].isTimedOut) {
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
  Serial.println("Initialisiere Sensor-Layouts...");
  
  struct SensorLayout {
    const char* label;
    const char* unit;
    int x, y, w, h;
    bool hasProgressBar;
    bool hasIndicator;
    bool hasBidirectionalBar;
  };
  
  // 3x3 Grid Layout - 8-Box Anordnung (untere rechte Ecke entfernt)
  const SensorLayout layouts[System::SENSOR_COUNT] = {
    // Reihe 1: Markt/Finanzen-Gruppe                                                                ProgressBar  Indicator  BidirectionalBar
    {"Oekostrom", "%",  10,  35, Layout::SENSOR_BOX_WIDTH, Layout::SENSOR_BOX_HEIGHT, false, true,  false},   // [0]
    {"Preis",     "ct", 115, 35, Layout::SENSOR_BOX_WIDTH, Layout::SENSOR_BOX_HEIGHT, false, false, false},  // [1] 
    {"Aktie",     "EUR", 220, 35, Layout::SENSOR_BOX_WIDTH, Layout::SENSOR_BOX_HEIGHT, false, false, false},  // [2] Aktie nach oben, EUR statt E
    // Reihe 2: Power/Charge-Gruppe  
    {"Ladestand", "%",  10,  85, Layout::SENSOR_BOX_WIDTH, Layout::SENSOR_BOX_HEIGHT, true,  false, false},  // [3]
    {"Verbrauch", "kW", 115, 85, Layout::SENSOR_BOX_WIDTH, Layout::SENSOR_BOX_HEIGHT, false, false, true}, // [4] Consumption Bar
    {"PV-Erzeugung", "W",  220, 85, Layout::SENSOR_BOX_WIDTH, Layout::SENSOR_BOX_HEIGHT, false, false, false},  // [5] PV-Erzeugung (war Wallbox)
    // Reihe 3: Umwelt-Gruppe (ohne untere rechte Ecke)
    {"Aussen",    "C",  10,  135, Layout::SENSOR_BOX_WIDTH, Layout::SENSOR_BOX_HEIGHT, false, false, false}, // [6]
    {"Wasser",    "C",  115, 135, Layout::SENSOR_BOX_WIDTH, Layout::SENSOR_BOX_HEIGHT, false, true,  false}  // [7] Wasser nach links
  };
  
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    const auto& layout = layouts[i];
    auto& sensor = sensors[i];
    
    strncpy(sensor.label, layout.label, sizeof(sensor.label) - 1);
    strncpy(sensor.unit, layout.unit, sizeof(sensor.unit) - 1);
    
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
      
    case 1: // Strompreis ct/kWh
      return value >= 0.0f && value <= 100.0f;
      
    case 2: // Wallbox W
      return value >= 0.0f && value <= 50000.0f; // Max 50kW
      
    case 3: // Ladestand %
      return value >= 0.0f && value <= 100.0f;
      
    case 4: // Wassertemperatur °C
      return value >= 0.0f && value <= 100.0f;
      
    case 5: // Außentemperatur °C
      return value >= -50.0f && value <= 70.0f;
      
    case 6: // Aktienkurs €
      return value >= 0.0f && value <= 10000.0f; // Plausible Obergrenze
      
    default:
      return true; // Unbekannte Sensoren: keine Validierung
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
        
      case 1: // Strompreis
        if (value < 0.0f) value = 0.0f;
        if (value > 100.0f) value = 100.0f;
        break;
        
      case 2: // Wallbox
        if (value < 0.0f) value = 0.0f;
        if (value > 50000.0f) value = 50000.0f;
        break;
        
      case 4: // Wassertemperatur
        value = constrain(value, 0.0f, 100.0f);
        break;
        
      case 5: // Außentemperatur
        value = constrain(value, -50.0f, 70.0f);
        break;
        
      case 6: // Aktienkurs
        if (value < 0.0f) value = 0.0f;
        if (value > 10000.0f) value = 10000.0f;
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
    
    if (!sensors[i].isTimedOut) {
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
                 i, sensors[i].label, stats.totalUpdates, 
                 stats.timeoutEvents, avgInterval.c_str());
    
    totalUpdates += stats.totalUpdates;
    totalTimeouts += stats.timeoutEvents;
  }
  
  Serial.println("└────┴─────────────┴─────────┴─────────┴──────────────┘");
  
  // Gesamtstatistik
  float systemUptime = (millis() - systemStartTime) / 1000.0f;
  float updateRate = totalUpdates / systemUptime;
  float reliabilityRate = (totalUpdates > 0) ? 
                         (1.0f - (float)totalTimeouts / totalUpdates) * 100.0f : 0.0f;
  
  Serial.printf("Gesamt: %lu Updates, %.1f Updates/s, %.1f%% Zuverlässigkeit\n",
               totalUpdates, updateRate, reliabilityRate);
}