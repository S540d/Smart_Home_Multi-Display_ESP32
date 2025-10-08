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

// MQTT Topics sind als Konstanten direkt in der Struktur in config.h definiert

// Hardware-Objekte
TFT_eSPI tft = TFT_eSPI();
WiFiClient espClient;
PubSubClient client(espClient);


// Kalibrierungs-System wird in TouchManager integriert

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
float wallboxPower = 0.0f; // Wallbox-Leistung

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
unsigned long lastViewChangeTime = 0;  // Für Auto-Return nach 10s

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

  // Watchdog aktivieren für System-Stabilität (30s Timeout)
  esp_task_wdt_init(30, true); // 30 Sekunden Timeout, enable panic
  esp_task_wdt_add(NULL);      // Add current task to watchdog

  // MQTT Buffer Size für Day-Ahead JSON Arrays erhöhen (Standard: 256 Bytes)
  client.setBufferSize(1024);
  Serial.printf("MQTT Buffer Size auf 1024 Bytes erhöht\n");

  systemStartTime = millis();
  
  Serial.println();
  Serial.println("╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║               ESP32 Smart Home Display                     ║");
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
    // Watchdog füttern um Reset zu vermeiden
    esp_task_wdt_reset();
    
    // OTA-Updates verarbeiten (höchste Priorität)
    handleOTA();

    // Touch-Init wurde bereits in setup() durchgeführt

    // MQTT Verbindung verwalten
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();

    // Prüfe zuerst ob Kalibrierung aktiv ist
    if (touchManager.isCalibrating()) {
      if (touchManager.updateCalibration()) {
        // Kalibrierung hat Display geändert - force redraw
        renderManager.markFullRedrawRequired();
      }
    } else {
      // Normale Touch-Events verarbeiten
      TouchEvent touchEvent = processTouchInput();
      if (touchEvent.type != TOUCH_NONE) {
        handleTouchEvent(touchEvent);
      }
    }

    // Touch-Marker deaktiviert für bessere Performance
    // updateTouchMarkers();
    
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
    
    // Auto-Return zur Hauptseite nach 10s (nur wenn nicht auf Hauptseite)
    if (currentMode != HOME_SCREEN && lastViewChangeTime > 0) {
      if (now - lastViewChangeTime >= 10000) {  // 10 Sekunden
        Serial.println("⏱️ Auto-Return zur Hauptseite nach 10s");
        currentMode = HOME_SCREEN;
        lastViewChangeTime = 0;
        renderManager.markFullRedrawRequired();
      }
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
  // Überschrift entfernt auf User-Anfrage
  tft.drawString("Initialisierung...", 10, 30, 1);

  logExecutionTime(startTime, "Display-Initialisierung");
}

void initializeTime() {
  unsigned long startTime = millis();

  if (!systemStatus.wifiConnected) {
    Serial.println("⚠️ Zeit-Sync übersprungen - kein WiFi");
    systemStatus.timeValid = false;
    return;
  }

  Serial.println("🕐 Starte NTP-Zeitsynchronisation...");
  
  // Konfiguriere NTP sofort nach WiFi-Verbindung
  configTime(0, 0, System::NTP_SERVER);
  setenv("TZ", System::TIMEZONE, 1);
  tzset();

  // Versuche Zeit-Sync mit verbesserter Strategie
  int attempts = 0;
  const int maxAttempts = 5;  // Mehr Versuche für bessere Erfolgschance
  
  while (!systemStatus.timeValid && attempts < maxAttempts) {
    systemStatus.updateTime();
    if (!systemStatus.timeValid) {
      delay(200);  // Kürzere Wartezeit für schnellere Initialisierung
      yield();
      attempts++;
    }
  }

  if (systemStatus.timeValid) {
    Serial.printf("✅ Zeit erfolgreich synchronisiert nach %d Versuchen\n", attempts + 1);
  } else {
    Serial.println("⚠️ Zeit-Sync fehlgeschlagen - wird im Hintergrund fortgesetzt");
  }

  logExecutionTime(startTime, "Zeit-Synchronisation");
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
  // Touch-Markierung deaktiviert für bessere Performance
  // if (event.type != TOUCH_MOVE && event.type != TOUCH_NONE) {
  //   drawTouchMarker(event.point.x, event.point.y);
  // }

  switch (event.type) {
    case TOUCH_DOWN:
      // Touch wurde gestartet
      if (event.sensorIndex >= 0) {
        onSensorTouched(event.sensorIndex);
      }
      break;

    case TOUCH_UP:
      // Touch wurde beendet - hier könnten Click-Actions implementiert werden

      // ERST: Prüfe Zurück-Button auf allen Detail-Screens (hat Vorrang)
      if (currentMode == DAYAHEAD_SCREEN ||
          currentMode == OEKOSTROM_DETAIL_SCREEN ||
          currentMode == WALLBOX_CONSUMPTION_SCREEN ||
          currentMode == LADESTAND_SCREEN ||
          currentMode == SETTINGS_SCREEN) {
        int backButtonX = 270 + antiBurnin.getOffsetX();
        int backButtonY = 10 + antiBurnin.getOffsetY();

        Serial.printf("🔍 Touch bei (%d,%d), Zurück-Button bei (%d,%d) bis (%d,%d)\n",
                     event.point.x, event.point.y, backButtonX, backButtonY,
                     backButtonX + 40, backButtonY + 20);

        // Erweiterte Touch-Area für bessere Erkennung (besonders bei Touch-Kalibrierung)
        int touchMargin = 15;  // Erhöht von 10 auf 15 für bessere Erkennung
        if (event.point.x >= (backButtonX - touchMargin) && event.point.x <= (backButtonX + 40 + touchMargin) &&
            event.point.y >= (backButtonY - touchMargin) && event.point.y <= (backButtonY + 20 + touchMargin)) {
          Serial.println("✅ Zurück-Button erkannt - zurück zum Home-Screen");
          currentMode = HOME_SCREEN;
          renderManager.markFullRedrawRequired();
          break; // Wichtig: Weitere Touch-Verarbeitung überspringen
        }
      }

      // Prüfe Settings-Screen Kalibrierungs-Buttons
      if (currentMode == SETTINGS_SCREEN) {
        if (touchManager.isCalibrating()) {
          // Beenden-Button während Kalibrierung
          int stopButtonX = 170 + antiBurnin.getOffsetX();
          int stopButtonY = 90;
          int stopButtonW = 100;
          int stopButtonH = 30;

          if (event.point.x >= stopButtonX && event.point.x <= stopButtonX + stopButtonW &&
              event.point.y >= stopButtonY && event.point.y <= stopButtonY + stopButtonH) {
            touchManager.stopCalibration();
            renderManager.markFullRedrawRequired();
            break;
          }
        } else {
          // Start-Button wenn nicht kalibriert
          int calibButtonX = 10 + antiBurnin.getOffsetX();
          int calibButtonY = 90;
          int calibButtonW = 150;
          int calibButtonH = 30;

          if (event.point.x >= calibButtonX && event.point.x <= calibButtonX + calibButtonW &&
              event.point.y >= calibButtonY && event.point.y <= calibButtonY + calibButtonH) {
            touchManager.startCalibration();
            renderManager.markFullRedrawRequired();
            break;
          }
        }
      }

      // DANN: Normale Sensor-Touch-Verarbeitung (nur auf Home-Screen)
      if (event.sensorIndex >= 0 && currentMode == HOME_SCREEN) {
        Serial.printf("🔍 Sensor Touch: Index=%d bei (%d,%d)\n",
                     event.sensorIndex, event.point.x, event.point.y);

        switch (event.sensorIndex) {
          case 0: // Ökostrom-Box - Wechsel zur Ökostrom-Detail-Ansicht
            currentMode = OEKOSTROM_DETAIL_SCREEN;
            lastViewChangeTime = millis();  // Timer für Auto-Return starten
            renderManager.markFullRedrawRequired();
            break;

          case 1: // Preis-Box - Wechsel zur Day-Ahead-Ansicht
            currentMode = DAYAHEAD_SCREEN;
            lastViewChangeTime = millis();  // Timer für Auto-Return starten
            renderManager.markFullRedrawRequired();
            break;

          case 3: // Ladestand-Box - Wechsel zur Ladestand-Ansicht
            currentMode = LADESTAND_SCREEN;
            lastViewChangeTime = millis();  // Timer für Auto-Return starten
            renderManager.markFullRedrawRequired();
            break;

          case 4: // Verbrauch-Box - Wechsel zur Verbrauch-Ansicht (Wallbox Consumption)
            currentMode = WALLBOX_CONSUMPTION_SCREEN;
            lastViewChangeTime = millis();  // Timer für Auto-Return starten
            renderManager.markFullRedrawRequired();
            break;
        }
      }

      // Prüfe freie Ecke (Position [2,2]) für Settings
      if (currentMode == HOME_SCREEN) {
        int settingsX = 220 + antiBurnin.getOffsetX();
        int settingsY = 135;
        int settingsW = Layout::SENSOR_BOX_WIDTH;
        int settingsH = Layout::SENSOR_BOX_HEIGHT;

        if (event.point.x >= settingsX && event.point.x <= settingsX + settingsW &&
            event.point.y >= settingsY && event.point.y <= settingsY + settingsH) {
          currentMode = SETTINGS_SCREEN;
          lastViewChangeTime = millis();  // Timer für Auto-Return starten
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

