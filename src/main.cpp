#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <time.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "display.h"
#include "network.h"
#include "sensors.h"
#include "utils.h"
#include "ota.h"
#include "touch.h"

// ═══════════════════════════════════════════════════════════════════════════════
//                              GLOBALE OBJEKTE UND VARIABLEN
// ═══════════════════════════════════════════════════════════════════════════════

// String-Konstanten Definitionen (genau einmal definiert)
const char* const System::NTP_SERVER = "pool.ntp.org";
const char* const System::TIMEZONE = "CET-1CEST,M3.5.0,M10.5.0/3";

// CONFIGURATION: Copy config_template.h to config_secrets.h and fill in your credentials
#if __has_include("config_secrets.h")
  #include "config_secrets.h"
  const char* const NetworkConfig::WIFI_SSID = CONFIG_WIFI_SSID;
  const char* const NetworkConfig::WIFI_PASSWORD = CONFIG_WIFI_PASSWORD;
  const char* const NetworkConfig::MQTT_SERVER = CONFIG_MQTT_SERVER;
#else
  #error "\n\n*** CONFIGURATION REQUIRED ***\nPlease copy 'config_template.h' to 'src/config_secrets.h' and configure your WiFi and MQTT settings.\n\nExample:\n  cp config_template.h src/config_secrets.h\n  # Then edit src/config_secrets.h with your credentials\n"
#endif
const char* const NetworkConfig::MQTT_USER = "";
const char* const NetworkConfig::MQTT_PASS = "";

// MQTT Topics Definition
const NetworkConfig::MqttTopics NetworkConfig::topics;

// Hardware-Objekte
TFT_eSPI tft = TFT_eSPI();
WiFiClient espClient;
PubSubClient client(espClient);

// Daten-Container
SensorData sensors[System::SENSOR_COUNT];
SystemStatus systemStatus;
AntiBurninManager antiBurnin;
RenderManager renderManager;

// Zusätzliche Daten
float stockReference = 0.0f;
float stockPreviousClose = 0.0f; // Vorheriger Schlusskurs für Prozentberechnung

// Power Management Variablen (alle in kW, immer positiv)
float pvPower = 0.0f;      // PV-Erzeugung
float gridPower = 0.0f;    // Netz-Leistung (Betrag)
float loadPower = 0.0f;    // Hausverbrauch
float storagePower = 0.0f; // Speicher-Leistung (Betrag)

// Letzte gültige Werte für stabile Berechnungen
LastValidPowerValues lastValidPower;

// Richtungsinformationen (berechnet)
bool isGridFeedIn = false;      // true = Einspeisung, false = Bezug
bool isStorageCharging = false; // true = Laden, false = Entladen

// Timing-Variablen
unsigned long lastSystemUpdate = 0;
unsigned long lastTimeoutCheck = 0;
unsigned long lastWifiReconnect = 0;
unsigned long lastLDROutput = 0;
unsigned long systemStartTime = 0;

// Touch-Init-Variablen (delayed init to avoid bootloop)
bool touchInitAttempted = false;
unsigned long touchInitDelay = 10000; // 10 Sekunden nach Boot

// ADC-Stabilitäts-Tracking für Pins 34-39
struct ADCStats {
  int totalVariation = 0;
  int sampleCount = 0;
  int lastValue = 0;
  float averageVariation = 0.0f;
} adcStats34, adcStats35, adcStats36, adcStats37, adcStats38, adcStats39;

// Display-Modus
DisplayMode currentMode = HOME_SCREEN;

// ═══════════════════════════════════════════════════════════════════════════════
//                              FUNKTIONS-PROTOTYPEN
// ═══════════════════════════════════════════════════════════════════════════════

void initializeSystem();
void initializeDisplay();
void initializeChipInfo();
void initializeTime();
void updateSystemStatus();
void handleLowMemory();
void handleCriticalError(const char* error);
void handleTouchEvent(const TouchEvent& event);

// ═══════════════════════════════════════════════════════════════════════════════
//                              SETUP UND MAIN LOOP
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Watchdog deaktivieren während Setup
  esp_task_wdt_init(30, false); // 30 Sekunden Timeout, kein Panic
  
  systemStartTime = millis();
  
  Serial.println();
  Serial.println("╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║               ESP32 Smart Home Display Rev2                ║");
  Serial.println("║            Vollständig modular strukturiert                 ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  
  try {
    Serial.println("Initialisiere System...");
    initializeSystem();
    
    Serial.println("Initialisiere Display...");
    initializeDisplay();
    
    Serial.println("Initialisiere Sensor-Layouts...");
    initializeSensorLayouts();
    
    Serial.println("💾 Ermittle Hardware-Info...");
    initializeChipInfo();
    
    Serial.println("Starte WiFi-Verbindung...");
    connectWiFi();
    
    Serial.println("🕐 Initialisiere Zeit-Service...");
    initializeTime();
    
    Serial.println("🔄 Aktiviere OTA-Updates...");
    initializeOTA();

    Serial.println("🎯 Touch-Controller...");
    Serial.println("⚠️ Touch-Init temporär deaktiviert wegen Bootschleife-Problem");
    Serial.println("   System läuft ohne Touch weiter");

    Serial.println("📧 Konfiguriere MQTT...");
    client.setServer(NetworkConfig::MQTT_SERVER, NetworkConfig::MQTT_PORT);
    client.setCallback(onMqttMessage);
    
    Serial.println("Erstelle initiales Display...");
    renderManager.markFullRedrawRequired();
    updateDisplay();
    
    // Erweiterte System-Diagnostik aus utils.cpp
    Serial.println("Erstelle System-Diagnostik...");
    logSystemInfo();
    logSensorStatus();
    
    Serial.println("System erfolgreich initialisiert!");
    
  } catch (const std::exception& e) {
    handleCriticalError("Setup-Fehler");
  }
}

void loop() {
  unsigned long now = millis();
  
  try {
    // OTA-Updates verarbeiten (höchste Priorität)
    handleOTA();

    // Touch-Init verzögert nach System-Start (Bootschleife vermeiden)
    if (!touchInitAttempted && (now - systemStartTime) > touchInitDelay) {
      touchInitAttempted = true;
      Serial.println("🎯 Versuche verzögerte Touch-Initialisierung...");

      try {
        if (initializeTouch()) {
          Serial.println("✅ Touch-Controller nachträglich initialisiert");
        } else {
          Serial.println("⚠️ Touch-Controller nicht verfügbar - System läuft ohne Touch weiter");
        }
      } catch (...) {
        Serial.println("❌ Touch-Init fehlgeschlagen - Touch bleibt deaktiviert");
      }
    }

    // MQTT Verbindung verwalten
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();

    // Touch-Events verarbeiten
    TouchEvent touchEvent = processTouchInput();
    if (touchEvent.type != TOUCH_NONE) {
      handleTouchEvent(touchEvent);
    }
    
    // Periodische Updates
    if (now - lastSystemUpdate >= Timing::SYSTEM_UPDATE_INTERVAL) {
      lastSystemUpdate = now;
      updateSystemStatus();
    }
    
    if (now - lastTimeoutCheck >= Timing::TIMEOUT_CHECK_INTERVAL) {
      lastTimeoutCheck = now;
      checkSensorTimeouts();
    }
    
    // ADC-Test entfernt - war nur für Hardware-Debugging
    
    // Anti-Burnin Management
    antiBurnin.update();
    if (antiBurnin.hasOffsetChanged()) {
      renderManager.markAntiBurninChanged();
    }
    
    // Display Updates
    if (renderManager.needsUpdate()) {
      updateDisplay();
      renderManager.lastRenderUpdate = now;
    }
    
    // System-Überwachung
    if (systemStatus.criticalMemoryWarning) {
      handleLowMemory();
    }
    
    // Erweiterte Performance-Berichte (alle 10 Minuten)
    static unsigned long lastReport = 0;
    if (now - lastReport >= 600000) {
      lastReport = now;
      updateSensorPerformance();
      logPerformanceStats();
      logSystemHealth();
    }
    
  } catch (const std::exception& e) {
    Serial.printf("WARNUNG - Loop-Fehler: %s\n", e.what());
    delay(1000);
  }
  
  delay(50);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              INITIALISIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

void initializeSystem() {
  Serial.println("🔧 Initialisiere System...");
  EEPROM.begin(512);
  randomSeed(analogRead(0));
  systemStatus.updateMemoryStatus();
  
  Serial.printf("   Freier Heap: %s\n", formatMemoryValue(systemStatus.freeHeap).c_str());
  logFreeHeap("System-Init");
}

void initializeDisplay() {
  Serial.println("Initialisiere Display...");
  
  unsigned long startTime = millis();
  
  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(false);
  tft.fillScreen(Colors::BG_MAIN);
  
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Smart Home Display Rev2", 10, 10, 2);
  tft.drawString("Initialisierung...", 10, 30, 1);
  
  logExecutionTime(startTime, "Display-Initialisierung");
  Serial.println("   Display bereit (320x240)");
}

void initializeTime() {
  Serial.println("🕐 Initialisiere Zeit...");
  
  unsigned long startTime = millis();
  
  // Nur wenn WiFi verbunden ist, versuche NTP-Synchronisation
  if (!systemStatus.wifiConnected) {
    Serial.println("   ⏩ Überspringe NTP - kein WiFi");
    systemStatus.timeValid = false;
    return;
  }
  
  Serial.println("   Starte NTP-Synchronisation...");
  configTime(0, 0, System::NTP_SERVER);
  setenv("TZ", System::TIMEZONE, 1);
  tzset();
  
  // Schnellere Zeit-Synchronisation (max 3 Sekunden)
  int attempts = 0;
  while (!systemStatus.timeValid && attempts < 3) {
    systemStatus.updateTime();
    if (!systemStatus.timeValid) {
      delay(500); // Kürzere Delays
      yield(); // Watchdog füttern
      attempts++;
      Serial.printf("   NTP-Versuch %d/3...\n", attempts);
    }
  }
  
  logExecutionTime(startTime, "Zeit-Synchronisation");
  
  if (systemStatus.timeValid) {
    Serial.printf("   Zeit synchronisiert: %s %s\n", 
                 systemStatus.currentDate, systemStatus.currentTime);
  } else {
    Serial.println("   WARNUNG - NTP-Timeout - System läuft mit lokaler Zeit");
    systemStatus.timeValid = false;
  }
}

void initializeChipInfo() {
  Serial.println("💾 Ermittle Hardware-Info...");
  
  auto& info = systemStatus.chipInfo;
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  
  info.chipRevision = chip_info.revision;
  info.chipId = ESP.getEfuseMac();
  info.hasPSRAM = (ESP.getPsramSize() > 0);
  info.psramSize = ESP.getPsramSize();
  info.flashSize = ESP.getFlashChipSize();
  
  Serial.printf("   ESP32 Rev.%d, Flash: %s", 
               info.chipRevision, 
               formatMemoryValue(info.flashSize).c_str());
               
  if (info.hasPSRAM) {
    Serial.printf(", PSRAM: %s", formatMemoryValue(info.psramSize).c_str());
  }
  Serial.println();
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              SYSTEM-MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void updateSystemStatus() {
  systemStatus.updateMemoryStatus();
  
  // Netzwerk-Status über network.cpp verwalten
  bool wasWifiConnected = systemStatus.wifiConnected;
  if (checkWifiConnection() != wasWifiConnected) {
    if (!systemStatus.wifiConnected) {
      reconnectWiFi();
    }
  }
  
  systemStatus.mqttConnected = client.connected();
  
  if (systemStatus.wifiConnected) {
    systemStatus.wifiRSSI = WiFi.RSSI();
  }
  
  // Zeit aktualisieren mit Change-Detection
  char oldTime[9];
  strcpy(oldTime, systemStatus.currentTime);
  systemStatus.updateTime();
  
  if (strcmp(oldTime, systemStatus.currentTime) != 0) {
    renderManager.markTimeChanged();
  }
  
  // CPU-Last simulieren
  float rawCpuUsage = random(5, 85);
  systemStatus.updateCpuUsage(rawCpuUsage);
  
  // Hardware-Sensoren (mit Filterung aus utils.cpp)
  systemStatus.ldrValue = readADCFiltered(System::LDR_PIN, 3);
  systemStatus.uptime = (millis() - systemStartTime) / 1000;
  
  renderManager.markSystemInfoChanged();
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              FEHLERBEHANDLUNG
// ═══════════════════════════════════════════════════════════════════════════════

void handleLowMemory() {
  Serial.println("KRITISCHER SPEICHERMANGEL!");
  logMemoryStatus();
  
  Serial.println("   → Führe Garbage Collection durch...");
  
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    sensors[i].requiresRedraw = true;
  }
  
  memset(&systemStatus.performance, 0, sizeof(systemStatus.performance));
  
  // Calendar functionality removed
  
  delay(100);
  systemStatus.updateMemoryStatus();
  
  Serial.printf("   → Nach Cleanup: %s verfügbar\n", 
               formatMemoryValue(systemStatus.freeHeap).c_str());
  
  if (!isSystemStable()) {
    Serial.println("WARNUNG - System instabil - Neustart empfohlen!");
    scheduleRestart(10000);
  }
}

void handleCriticalError(const char* error) {
  Serial.printf("KRITISCHER FEHLER: %s\n", error);
  
  // Erweiterte Fehler-Diagnostik
  generateSystemReport();
  
  tft.fillScreen(Colors::STATUS_RED);
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("SYSTEM ERROR", 10, 10, 2);
  tft.drawString(error, 10, 40, 1);
  tft.drawString("Neustart in 10s...", 10, 60, 1);
  
  // Cleanup und Restart über utils.cpp
  scheduleRestart(10000);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              TOUCH-EVENT-HANDLER
// ═══════════════════════════════════════════════════════════════════════════════

void handleTouchEvent(const TouchEvent& event) {
  switch (event.type) {
    case TOUCH_DOWN:
      // Touch wurde gestartet
      if (event.sensorIndex >= 0) {
        onSensorTouched(event.sensorIndex);
      }
      break;

    case TOUCH_UP:
      // Touch wurde beendet - hier könnten Click-Actions implementiert werden
      if (event.sensorIndex >= 0) {
        Serial.printf("🖱️ Click auf Sensor %d\n", event.sensorIndex);
        // Future: Sensor-spezifische Click-Aktionen
      }
      break;

    case TOUCH_LONG_PRESS:
      onLongPress(event.point);
      break;

    case TOUCH_DOUBLE_TAP:
      Serial.printf("👆👆 Double Tap bei (%d,%d)\n", event.point.x, event.point.y);
      if (event.sensorIndex >= 0) {
        Serial.printf("🔧 Sensor %d Detail-Ansicht\n", event.sensorIndex);
        // Future: Open sensor detail view
      }
      break;

    case SWIPE_LEFT:
    case SWIPE_RIGHT:
    case SWIPE_UP:
    case SWIPE_DOWN:
      onGestureDetected(event.type);
      break;

    case TOUCH_MOVE:
      // Touch-Bewegung - für Drag-Operationen
      break;

    default:
      break;
  }

  // Touch-Bereiche nach Anti-Burnin-Änderungen aktualisieren
  static unsigned long lastTouchAreaUpdate = 0;
  if (millis() - lastTouchAreaUpdate > 5000) { // Alle 5 Sekunden prüfen
    if (antiBurnin.hasOffsetChanged()) {
      touchManager.updateSensorTouchAreas();
      lastTouchAreaUpdate = millis();
    }
  }
}

