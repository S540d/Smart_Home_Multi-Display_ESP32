#include "utils.h"
#include <WiFi.h>
static SystemState& appState = SystemState::getInstance();
  // Für WiFi.localIP() und WiFi-Funktionen

// ═══════════════════════════════════════════════════════════════════════════════
//                              STRING-FORMATIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

String formatUptime(unsigned long seconds) {
  if (seconds < 60) {
    return String(seconds) + "s";
  } else if (seconds < 3600) {
    return String(seconds / 60) + "m " + String(seconds % 60) + "s";
  } else if (seconds < 86400) {
    unsigned long hours = seconds / 3600;
    unsigned long mins = (seconds % 3600) / 60;
    return String(hours) + "h " + String(mins) + "m";
  } else {
    unsigned long days = seconds / 86400;
    unsigned long hours = (seconds % 86400) / 3600;
    return String(days) + "d " + String(hours) + "h";
  }
}

String formatTime(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long ms = milliseconds % 1000;
  
  if (seconds == 0) {
    return String(ms) + "ms";
  } else if (seconds < 60) {
    return String(seconds) + "." + String(ms / 100) + "s";
  } else {
    return formatUptime(seconds);
  }
}

String formatDuration(unsigned long startTime, unsigned long endTime) {
  if (endTime < startTime) {
    return "Invalid duration";
  }
  return formatTime(endTime - startTime);
}

String formatMemoryValue(uint32_t bytes) {
  if (bytes >= 1024 * 1024) {
    return String(bytes / (1024.0f * 1024.0f), 1) + "MB";
  } else if (bytes >= 1024) {
    return String(bytes / 1024.0f, 1) + "KB";
  } else {
    return String(bytes) + "B";
  }
}

String formatMemoryPercentage(uint32_t used, uint32_t total) {
  if (total == 0) return "0%";
  float percentage = (float)used / total * 100.0f;
  return String(percentage, 1) + "%";
}

String formatFloat(float value, int decimals) {
  if (isnan(value)) return "NaN";
  if (isinf(value)) return value > 0 ? "∞" : "-∞";
  return String(value, decimals);
}

String formatPercentage(float value, int decimals) {
  return formatFloat(value, decimals) + "%";
}

String formatFileSize(size_t bytes) {
  const char* units[] = {"B", "KB", "MB", "GB"};
  int unitIndex = 0;
  float size = bytes;
  
  while (size >= 1024 && unitIndex < 3) {
    size /= 1024;
    unitIndex++;
  }
  
  return String(size, (unitIndex == 0) ? 0 : 1) + units[unitIndex];
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              SYSTEM-LOGGING
// ═══════════════════════════════════════════════════════════════════════════════

void logSystemInfo() {
  Serial.println();
  Serial.println("╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                        SYSTEM-INFO                          ║");
  Serial.println("╠══════════════════════════════════════════════════════════════╣");
  
  auto& info = appState.systemStatus.chipInfo;
  Serial.printf("║ Hardware    : ESP32 Rev.%d (ID: 0x%08X)                     ║\n", 
               info.chipRevision, (uint32_t)info.chipId);
  Serial.printf("║ Flash       : %s                                           ║\n", 
               formatMemoryValue(info.flashSize).c_str());
  
  if (info.hasPSRAM) {
    Serial.printf("║ PSRAM       : %s                                           ║\n", 
                 formatMemoryValue(info.psramSize).c_str());
  } else {
    Serial.println("║ PSRAM       : Nicht verfügbar                               ║");
  }
  
  Serial.printf("║ Freier Heap : %s (Min: %s)                                ║\n", 
               formatMemoryValue(appState.systemStatus.freeHeap).c_str(),
               formatMemoryValue(appState.systemStatus.minFreeHeap).c_str());
  Serial.printf("║ WiFi        : %s (RSSI: %d dBm)                            ║\n", 
               appState.systemStatus.wifiConnected ? "Verbunden" : "Getrennt", 
               appState.systemStatus.wifiRSSI);
  Serial.printf("║ MQTT        : %s                                           ║\n", 
               appState.systemStatus.mqttConnected ? "Verbunden" : "Getrennt");
  Serial.printf("║ Zeit        : %s %s                                       ║\n", 
               appState.systemStatus.currentDate, appState.systemStatus.currentTime);
  Serial.printf("║ Uptime      : %s                                           ║\n",
               formatUptime(appState.systemStatus.uptime).c_str());
  
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  Serial.println();
}

void logMemoryStatus() {
  Serial.printf("Memory: %s frei (Min: %s, %.1f%% belegt)", 
               formatMemoryValue(appState.systemStatus.freeHeap).c_str(),
               formatMemoryValue(appState.systemStatus.minFreeHeap).c_str(),
               (1.0f - (float)appState.systemStatus.freeHeap / (appState.systemStatus.freeHeap + 100000)) * 100.0f);
  
  if (appState.systemStatus.criticalMemoryWarning) {
    Serial.print(" KRITISCH!");
  } else if (appState.systemStatus.lowMemoryWarning) {
    Serial.print(" NIEDRIG!");
  } else {
    Serial.print(" OK");
  }
  Serial.println();
}

void logPerformanceStats() {
  auto& perf = appState.systemStatus.performance;
  unsigned long total = perf.totalRedraws + perf.skippedRedraws;
  
  Serial.println();
  Serial.println("PERFORMANCE-STATISTIKEN:");
  Serial.printf("   CPU-Last (geglättet): %.1f%%\n", appState.systemStatus.cpuUsageSmoothed);
  Serial.printf("   Redraws total: %lu\n", perf.totalRedraws);
  Serial.printf("   Redraws übersprungen: %lu\n", perf.skippedRedraws);
  Serial.printf("   Render-Effizienz: %.1f%%\n", perf.redrawEfficiency);
  Serial.printf("   Uptime: %s\n", formatUptime(appState.systemStatus.uptime).c_str());
  Serial.printf("   LDR-Wert: %d (geglättet: %d)\n", appState.systemStatus.ldrValue, appState.systemStatus.ldrValueSmoothed);
  
  logMemoryStatus();
  Serial.println();
}

void logNetworkInfo() {
  Serial.println("🌐 NETZWERK-STATUS:");
  
  if (appState.systemStatus.wifiConnected) {
    Serial.printf("   WiFi: Verbunden (%s)\n", WiFi.localIP().toString().c_str());
    Serial.printf("   RSSI: %d dBm", appState.systemStatus.wifiRSSI);
    
    if (appState.systemStatus.wifiRSSI > -50) Serial.println(" (Exzellent)");
    else if (appState.systemStatus.wifiRSSI > -60) Serial.println(" (Sehr gut)");
    else if (appState.systemStatus.wifiRSSI > -70) Serial.println(" (Gut)");
    else Serial.println(" (Schwach)");
    
    Serial.printf("   Reconnect-Versuche: %lu\n", appState.systemStatus.wifiReconnectAttempts);
  } else {
    Serial.println("   WiFi: ❌ Getrennt");
  }
  
  if (appState.systemStatus.mqttConnected) {
    Serial.println("   MQTT: Verbunden");
  } else {
    Serial.printf("   MQTT: ❌ Getrennt (Versuche: %lu)\n", appState.systemStatus.mqttReconnectAttempts);
  }
  
  Serial.println();
}

void logFreeHeap(const char* location) {
  Serial.printf("Heap@%s: %s\n", location, formatMemoryValue(ESP.getFreeHeap()).c_str());
}

void logExecutionTime(unsigned long startTime, const char* operation) {
  unsigned long duration = millis() - startTime;
  Serial.printf("%s: %s\n", operation, formatTime(duration).c_str());
}

void logSystemHealth() {
  Serial.println("🏥 SYSTEM-GESUNDHEITSCHECK:");
  
  // Memory Health
  float memoryHealth = (float)appState.systemStatus.freeHeap / System::MIN_FREE_HEAP * 100.0f;
  memoryHealth = constrain(memoryHealth, 0.0f, 100.0f);
  Serial.printf("   Memory: %.1f%% ", memoryHealth);
  if (memoryHealth > 80) Serial.println("Gesund");
  else if (memoryHealth > 50) Serial.println("Warnung");
  else Serial.println("Kritisch");
  
  // Network Health
  bool networkHealth = appState.systemStatus.wifiConnected && appState.systemStatus.mqttConnected;
  Serial.printf("   Netzwerk: %s\n", networkHealth ? "Gesund" : "Probleme");
  
  // Performance Health
  float performanceHealth = 100.0f - appState.systemStatus.cpuUsageSmoothed;
  Serial.printf("   Performance: %.1f%% ", performanceHealth);
  if (performanceHealth > 70) Serial.println("Gesund");
  else if (performanceHealth > 40) Serial.println("Belastet");
  else Serial.println("Überlastet");
  
  // Overall Health Score
  float overallHealth = (memoryHealth + (networkHealth ? 100 : 0) + performanceHealth) / 3.0f;
  Serial.printf("   Gesamt: %.1f%% ", overallHealth);
  if (overallHealth > 80) Serial.println("System gesund");
  else if (overallHealth > 60) Serial.println("System beeinträchtigt");
  else Serial.println("System kritisch");
  
  Serial.println();
}

void generateSystemReport() {
  Serial.println();
  Serial.println("═══════════════════════════════════════════════════════════════");
  Serial.println("                     SYSTEM-REPORT");
  Serial.println("═══════════════════════════════════════════════════════════════");
  
  logSystemInfo();
  logNetworkInfo();
  logPerformanceStats();
  logSystemHealth();
  
  Serial.println("═══════════════════════════════════════════════════════════════");
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              MATHEMATISCHE HILFSFUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

float mapFloat(float value, float inMin, float inMax, float outMin, float outMax) {
  if (inMax == inMin) return outMin; // Avoid division by zero
  return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

float constrainFloat(float value, float minVal, float maxVal) {
  if (value < minVal) return minVal;
  if (value > maxVal) return maxVal;
  return value;
}

int roundToNearest(float value, int base) {
  return round(value / base) * base;
}

float calculateAverage(float values[], int count) {
  if (count <= 0) return 0.0f;
  
  float sum = 0.0f;
  for (int i = 0; i < count; i++) {
    sum += values[i];
  }
  return sum / count;
}

float calculateMovingAverage(float newValue, float oldAverage, float weight) {
  weight = constrainFloat(weight, 0.0f, 1.0f);
  return oldAverage * (1.0f - weight) + newValue * weight;
}

float calculatePercentageChange(float oldValue, float newValue) {
  if (oldValue == 0.0f) return 0.0f;
  return ((newValue - oldValue) / oldValue) * 100.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              UTILITY-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

void safeCopyString(char* dest, const char* src, size_t destSize) {
  if (dest == nullptr || src == nullptr || destSize == 0) return;
  
  strncpy(dest, src, destSize - 1);
  dest[destSize - 1] = '\0';
}

bool isValidString(const char* str, size_t maxLength) {
  if (str == nullptr) return false;
  
  size_t len = strlen(str);
  return len > 0 && len <= maxLength;
}

String trimString(const String& str) {
  String result = str;
  
  // Leading whitespace entfernen
  while (result.length() > 0 && isspace(result.charAt(0))) {
    result = result.substring(1);
  }
  
  // Trailing whitespace entfernen
  while (result.length() > 0 && isspace(result.charAt(result.length() - 1))) {
    result = result.substring(0, result.length() - 1);
  }
  
  return result;
}

void feedWatchdog() {
  // ESP32-kompatible Implementierung ohne Hardware-Watchdog
  yield(); // Gibt dem RTOS eine Chance zur Task-Umschaltung
  delay(1); // Kurze Pause für System-Tasks
}

void scheduleRestart(unsigned long delayMs) {
  Serial.printf("🔄 System-Neustart geplant in %s\n", formatTime(delayMs).c_str());
  
  // Cleanup vor Restart
  delay(delayMs);
  ESP.restart();
}

bool isSystemStable() {
  // System gilt als stabil wenn:
  // - Ausreichend Speicher verfügbar
  // - CPU-Last unter 90%
  // - Netzwerk funktioniert
  // - Keine kritischen Fehler
  
  bool memoryOk = appState.systemStatus.freeHeap > System::CRITICAL_HEAP_LEVEL;
  bool cpuOk = appState.systemStatus.cpuUsageSmoothed < 90.0f;
  bool networkOk = appState.systemStatus.wifiConnected;
  
  return memoryOk && cpuOk && networkOk && !appState.systemStatus.hasNetworkError;
}

int readADCFiltered(int pin, int samples) {
  if (samples <= 0) samples = 1;

  // Attenuation nur für diesen Pin setzen (0-3.3V Bereich), nicht global —
  // andere ADC-Nutzer bleiben unberührt
  analogSetPinAttenuation(pin, ADC_11db);

  // Mehrfach-Sampling mit Ausreißer-Filterung.
  // analogReadMilliVolts nutzt die eFuse-Werkskalibrierung und ist deutlich
  // stabiler als rohe analogRead-Werte; Ergebnis wird auf die bisherige
  // 0-4095-Skala zurückgerechnet, damit Anzeige/Logs kompatibel bleiben.
  int readings[samples];
  for (int i = 0; i < samples; i++) {
    uint32_t mv = analogReadMilliVolts(pin);
    readings[i] = (int)((mv * 4095UL) / 3300UL);
    // ~1 ms Abstand, damit ein einzelner WiFi-/I2C-Burst (Touch-Polling auf
    // GPIO 32/33 direkt neben dem LDR an GPIO 34) nicht alle Samples trifft
    if (samples > 1) delayMicroseconds(1000);
  }
  
  // Sortiere Werte für Median-Berechnung (robuster gegen Ausreißer)
  for (int i = 0; i < samples - 1; i++) {
    for (int j = i + 1; j < samples; j++) {
      if (readings[i] > readings[j]) {
        int temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }
  
  // Verwende Median (mittlerer Wert) anstatt Durchschnitt
  // Bei ungerader Anzahl: mittlerer Wert, bei gerader: Durchschnitt der 2 mittleren
  if (samples % 2 == 1) {
    return readings[samples / 2];
  } else {
    return (readings[samples / 2 - 1] + readings[samples / 2]) / 2;
  }
}

float getTemperatureFromADC(int adcValue) {
  // Beispiel-Konvertierung für NTC-Thermistor
  // Diese Funktion müsste an den spezifischen Sensor angepasst werden
  float voltage = (adcValue / 4095.0f) * 3.3f;
  
  // Vereinfachte lineare Approximation (ersetzt durch echte Kalibration)
  float temperature = mapFloat(voltage, 0.5f, 2.5f, -10.0f, 50.0f);
  
  return constrainFloat(temperature, -50.0f, 100.0f);
}

bool isValidWiFiSignal(int rssi) {
  // RSSI-Werte zwischen -100 und 0 dBm sind gültig
  return rssi >= -100 && rssi <= 0;
}