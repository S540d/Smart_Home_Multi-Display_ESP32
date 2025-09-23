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

// Price Detail Data
DayAheadPriceData dayAheadPrices;

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
    initializeSystem();
    initializeDisplay();
    initializeSensorLayouts();
    initializeChipInfo();
    connectWiFi();
    initializeTime();
    initializeOTA();
    initializeSensorLayouts();
    initializeTouch();

    client.setServer(NetworkConfig::MQTT_SERVER, NetworkConfig::MQTT_PORT);
    client.setCallback(onMqttMessage);

    renderManager.markFullRedrawRequired();
    updateDisplay();
    
    logSystemInfo();
    logSensorStatus();
    
  } catch (const std::exception& e) {
    handleCriticalError("Setup-Fehler");
  }
}

void loop() {
  unsigned long now = millis();
  
  try {
    // OTA-Updates verarbeiten (höchste Priorität)
    handleOTA();

    // Touch-Init wurde bereits in setup() durchgeführt

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

    // Touch-Marker aktualisieren (abgelaufene entfernen)
    updateTouchMarkers();
    
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
      // Touch-Bereiche nach Anti-Burnin-Änderung aktualisieren
      Serial.println("🔄 Anti-Burnin-Änderung - Aktualisiere Touch-Bereiche...");
      touchManager.updateSensorTouchAreas();
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
  EEPROM.begin(512);
  randomSeed(analogRead(0));
  systemStatus.updateMemoryStatus();
  logFreeHeap("System-Init");
}

void initializeDisplay() {
  unsigned long startTime = millis();

  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(false);
  tft.fillScreen(Colors::BG_MAIN);

  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Smart Home Display Rev2", 10, 10, 2);
  tft.drawString("Initialisierung...", 10, 30, 1);

  logExecutionTime(startTime, "Display-Initialisierung");
}

void initializeTime() {
  unsigned long startTime = millis();

  if (!systemStatus.wifiConnected) {
    systemStatus.timeValid = false;
    return;
  }

  configTime(0, 0, System::NTP_SERVER);
  setenv("TZ", System::TIMEZONE, 1);
  tzset();

  int attempts = 0;
  while (!systemStatus.timeValid && attempts < 3) {
    systemStatus.updateTime();
    if (!systemStatus.timeValid) {
      delay(500);
      yield();
      attempts++;
    }
  }

  logExecutionTime(startTime, "Zeit-Synchronisation");

  if (!systemStatus.timeValid) {
    systemStatus.timeValid = false;
  }
}

void initializeChipInfo() {
  auto& info = systemStatus.chipInfo;
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  info.chipRevision = chip_info.revision;
  info.chipId = ESP.getEfuseMac();
  info.hasPSRAM = (ESP.getPsramSize() > 0);
  info.psramSize = ESP.getPsramSize();
  info.flashSize = ESP.getFlashChipSize();
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
  logMemoryStatus();

  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    sensors[i].requiresRedraw = true;
  }

  memset(&systemStatus.performance, 0, sizeof(systemStatus.performance));

  delay(100);
  systemStatus.updateMemoryStatus();

  if (!isSystemStable()) {
    scheduleRestart(10000);
  }
}

void handleCriticalError(const char* error) {
  generateSystemReport();

  tft.fillScreen(Colors::STATUS_RED);
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("SYSTEM ERROR", 10, 10, 2);
  tft.drawString(error, 10, 40, 1);
  tft.drawString("Neustart in 10s...", 10, 60, 1);

  scheduleRestart(10000);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              TOUCH-EVENT-HANDLER
// ═══════════════════════════════════════════════════════════════════════════════

void handleTouchEvent(const TouchEvent& event) {
  // Touch-Markierung für ALLE Touch-Events anzeigen (außer MOVE)
  if (event.type != TOUCH_MOVE && event.type != TOUCH_NONE) {
    drawTouchMarker(event.point.x, event.point.y);
  }

  switch (event.type) {
    case TOUCH_DOWN:
      // Touch wurde gestartet
      if (event.sensorIndex >= 0) {
        onSensorTouched(event.sensorIndex);
      }
      break;

    case TOUCH_UP:
      // Touch wurde beendet - hier könnten Click-Actions implementiert werden

      // ERST: Prüfe Zurück-Button auf Preis-Detail-Screen (hat Vorrang)
      if (currentMode == PRICE_DETAIL_SCREEN) {
        int backButtonX = 270 + antiBurnin.getOffsetX();
        int backButtonY = 10;

        if (event.point.x >= backButtonX && event.point.x <= backButtonX + 40 &&
            event.point.y >= backButtonY && event.point.y <= backButtonY + 20) {
          currentMode = HOME_SCREEN;
          renderManager.markFullRedrawRequired();
          break; // Wichtig: Weitere Touch-Verarbeitung überspringen
        }
      }

      // DANN: Normale Sensor-Touch-Verarbeitung
      if (event.sensorIndex >= 0) {
        // Ökostrom-Box Touch (Sensor Index 0) - Wechsel zur Preis-Detail-Ansicht
        if (event.sensorIndex == 0 && currentMode == HOME_SCREEN) {
          currentMode = PRICE_DETAIL_SCREEN;
          renderManager.markFullRedrawRequired();
        }
      }
      break;

    case TOUCH_LONG_PRESS:
      onLongPress(event.point);
      break;

    case TOUCH_DOUBLE_TAP:
      if (event.sensorIndex >= 0) {
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
  if (millis() - lastTouchAreaUpdate > 5000) {
    if (antiBurnin.hasOffsetChanged()) {
      touchManager.updateSensorTouchAreas();
      lastTouchAreaUpdate = millis();
    }
  }
}

