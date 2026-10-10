#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <time.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "state.h"
#include "display.h"
#include "network.h"
#include "sensors.h"
#include "utils.h"
#include "ota.h"
#include "touch.h"
#include "tasks.h"

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

// Hardware-Objekte (bleiben global — keine Zustandsdaten, sondern Hardware-Handles)
LGFX tft;
WiFiClient espClient;
PubSubClient client(espClient);

// Alle Zustandsdaten zentral verwaltet
static SystemState& appState = SystemState::getInstance();

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
void notifyActivity();
void updateCpuScaling();
void publishPlugCommand(int plugIndex, bool newState);
void publishPlugCommandDirect(int plugIndex, bool newState);

// ═══════════════════════════════════════════════════════════════════════════════
//                              SETUP UND MAIN LOOP
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Watchdog aktivieren für System-Stabilität (60s Timeout für WiFi-Reconnect)
  esp_task_wdt_init(60, true); // 60 Sekunden Timeout, enable panic
  // loop() task ist NICHT im WDT - nur die 4 Custom-Tasks registrieren sich selbst

  // MQTT Buffer Size für Day-Ahead JSON Arrays (24h × ~15 Bytes/Eintrag ≈ 600+ Bytes)
  client.setBufferSize(2048);
  Serial.printf("MQTT Buffer Size auf 2048 Bytes gesetzt\n");

  appState.systemStartTime = millis();
  appState.begin();
  
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
    initializeTouch();

    client.setServer(NetworkConfig::MQTT_SERVER, NetworkConfig::MQTT_PORT);
    client.setCallback(onMqttMessage);

    appState.renderManager.markFullRedrawRequired();
    updateDisplay();

    logSystemInfo();
    logSensorStatus();

  } catch (const std::exception& e) {
    handleCriticalError("Setup-Fehler");
  }

  initializeTasks();
}

void loop() {
  // All work is done in the four FreeRTOS tasks.
  // loop() task sleeps forever to avoid consuming CPU.
  vTaskDelay(portMAX_DELAY);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              INITIALISIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

void initializeSystem() {
  EEPROM.begin(512);
  // esp_random() statt analogRead(0): GPIO 0 ist Strapping-Pin an ADC2,
  // und ADC2 kollidiert mit aktivem WiFi
  randomSeed(esp_random());
  appState.systemStatus.updateMemoryStatus();

  // SmartPlug-Namen initialisieren
  strncpy(appState.smartPlugs[0].name, "Drucker", sizeof(appState.smartPlugs[0].name) - 1);
  strncpy(appState.smartPlugs[1].name, "Plug 2", sizeof(appState.smartPlugs[1].name) - 1);

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

  if (!appState.systemStatus.wifiConnected) {
    Serial.println("⚠️ Zeit-Sync übersprungen - kein WiFi");
    appState.systemStatus.timeValid = false;
    return;
  }

  Serial.println("🕐 Starte NTP-Zeitsynchronisation...");

  // Konfiguriere NTP sofort nach WiFi-Verbindung
  configTime(0, 0, System::NTP_SERVER);
  setenv("TZ", System::TIMEZONE, 1);
  tzset();

  // Versuche Zeit-Sync mit verbesserter Strategie
  int attempts = 0;
  const int maxAttempts = 10;

  while (!appState.systemStatus.timeValid && attempts < maxAttempts) {
    appState.systemStatus.updateTime();
    if (!appState.systemStatus.timeValid) {
      delay(200);  // Kürzere Wartezeit für schnellere Initialisierung
      yield();
      attempts++;
    }
  }

  if (appState.systemStatus.timeValid) {
    Serial.printf("✅ Zeit erfolgreich synchronisiert nach %d Versuchen\n", attempts + 1);
  } else {
    Serial.println("⚠️ Zeit-Sync fehlgeschlagen - wird im Hintergrund fortgesetzt");
  }

  logExecutionTime(startTime, "Zeit-Synchronisation");
}

void initializeChipInfo() {
  auto& info = appState.systemStatus.chipInfo;
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
  // WiFi/MQTT reconnect is handled by networkTask — not here.
  appState.systemStatus.updateMemoryStatus();

  // Zeit aktualisieren mit Change-Detection
  char oldTime[9];
  strcpy(oldTime, appState.systemStatus.currentTime);
  appState.systemStatus.updateTime();

  if (strcmp(oldTime, appState.systemStatus.currentTime) != 0) {
    appState.renderManager.markTimeChanged();
  }

  // CPU-Last simulieren
  float rawCpuUsage = random(5, 85);
  appState.systemStatus.updateCpuUsage(rawCpuUsage);

  // Hardware-Sensoren (mit verbesserter Filterung)
  int rawLdr = readADCFiltered(System::LDR_PIN, 9);
  appState.systemStatus.ldrValue = rawLdr;
  // EMA läuft kontinuierlich weiter; der veröffentlichte Wert folgt ihr erst,
  // wenn sie sich um mehr als die Rauschschwelle entfernt hat (Deadband).
  // So flackert die Anzeige nicht, echte Änderungen kommen aber immer durch.
  appState.systemStatus.ldrEma = calculateMovingAverage(rawLdr, appState.systemStatus.ldrEma, 0.2f);
  if (abs((int)appState.systemStatus.ldrEma - appState.systemStatus.ldrValueSmoothed) >= System::LDR_DEADBAND) {
    appState.systemStatus.ldrValueSmoothed = (int)appState.systemStatus.ldrEma;
  }
  appState.systemStatus.uptime = (millis() - appState.systemStartTime) / 1000;

  appState.renderManager.markSystemInfoChanged();
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              FEHLERBEHANDLUNG
// ═══════════════════════════════════════════════════════════════════════════════

void handleLowMemory() {
  logMemoryStatus();

  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    appState.sensors[i].requiresRedraw = true;
  }

  memset(&appState.systemStatus.performance, 0, sizeof(appState.systemStatus.performance));

  delay(100);
  appState.systemStatus.updateMemoryStatus();

  if (!isSystemStable()) {
    scheduleRestart(10000);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              CPU-FREQUENZ-SKALIERUNG (Phase 2.1)
// ═══════════════════════════════════════════════════════════════════════════════

void notifyActivity() {
  // Called from touchTask and networkTask — must be mutex-safe.
  if (appState.stateMutex == nullptr) {
    appState.lastActivityTime = millis();
    return;
  }
  if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    appState.lastActivityTime = millis();
    bool wasIdle = appState.cpuIsIdle;
    appState.cpuIsIdle = false;
    xSemaphoreGiveRecursive(appState.stateMutex);
    if (wasIdle) {
      setCpuFrequencyMhz(CpuScaling::FREQ_ACTIVE_MHZ);
      Serial.printf("CPU: %uMHz (Aktivität erkannt)\n", CpuScaling::FREQ_ACTIVE_MHZ);
    }
  }
}

void updateCpuScaling() {
  unsigned long now = millis();
  if (!appState.cpuIsIdle && (now - appState.lastActivityTime) >= CpuScaling::IDLE_TIMEOUT_MS) {
    setCpuFrequencyMhz(CpuScaling::FREQ_IDLE_MHZ);
    appState.cpuIsIdle = true;
    Serial.printf("CPU: %uMHz (Idle nach %lus)\n",
                  CpuScaling::FREQ_IDLE_MHZ, CpuScaling::IDLE_TIMEOUT_MS / 1000);
  }
}

void publishPlugCommandDirect(int plugIndex, bool newState) {
  if (plugIndex < 0 || plugIndex >= SMART_PLUG_COUNT) return;

  if (!client.connected()) {
    Serial.printf("SmartPlug %d: MQTT nicht verbunden\n", plugIndex + 1);
    return;
  }

  const char* setTopics[SMART_PLUG_COUNT] = {
    NetworkConfig::PLUG1_SET, NetworkConfig::PLUG2_SET
  };
  const char* payload = newState ? "ON" : "OFF";
  if (client.publish(setTopics[plugIndex], payload)) {
    Serial.printf("SmartPlug %d Set: %s\n", plugIndex + 1, payload);
  } else {
    Serial.printf("SmartPlug %d Publish-Fehler\n", plugIndex + 1);
  }
}

void publishPlugCommand(int plugIndex, bool newState) {
  if (plugIndex < 0 || plugIndex >= SMART_PLUG_COUNT) return;
  if (plugCommandQueue == nullptr) {
    publishPlugCommandDirect(plugIndex, newState);
    return;
  }
  PlugCommand cmd = {plugIndex, newState};
  if (xQueueSend(plugCommandQueue, &cmd, pdMS_TO_TICKS(10)) != pdTRUE) {
    Serial.printf("WARNUNG: plugCommandQueue voll - SmartPlug %d Kommando verworfen\n", plugIndex + 1);
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
  // Garagentor-Alarm überdeckt alles: Tippen quittiert ihn, alle anderen Touch-
  // Events werden verschluckt, damit nichts "unter" dem Kasten ausgelöst wird.
  if (appState.garage.alertActive()) {
    if (event.type == TOUCH_UP) {
      appState.garage.dismissed = true;
      appState.garage.alertDrawn = false;
      appState.renderManager.markFullRedrawRequired();
      Serial.println("Garagentor-Alarm quittiert");
    }
    return;
  }

  // Lüften-Alarm: gleiches Verhalten wie der Garagentor-Alarm
  if (appState.air.alertActive()) {
    if (event.type == TOUCH_UP) {
      appState.air.dismissed = true;
      appState.air.alertDrawn = false;
      appState.renderManager.markFullRedrawRequired();
      Serial.println("Lüften-Alarm quittiert");
    }
    return;
  }

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
      if (appState.currentMode == DAYAHEAD_SCREEN ||
          appState.currentMode == OEKOSTROM_DETAIL_SCREEN ||
          appState.currentMode == WALLBOX_CONSUMPTION_SCREEN ||
          appState.currentMode == LADESTAND_SCREEN ||
          appState.currentMode == SETTINGS_SCREEN ||
          appState.currentMode == DEVICES_SCREEN) {
        int backButtonX = 270 + appState.antiBurnin.getOffsetX();
        int backButtonY = 10 + appState.antiBurnin.getOffsetY();

        Serial.printf("🔍 Touch bei (%d,%d), Zurück-Button bei (%d,%d) bis (%d,%d)\n",
                     event.point.x, event.point.y, backButtonX, backButtonY,
                     backButtonX + 40, backButtonY + 20);

        // Erweiterte Touch-Area für bessere Erkennung (besonders bei Touch-Kalibrierung)
        int touchMargin = 15;  // Erhöht von 10 auf 15 für bessere Erkennung
        if (event.point.x >= (backButtonX - touchMargin) && event.point.x <= (backButtonX + 40 + touchMargin) &&
            event.point.y >= (backButtonY - touchMargin) && event.point.y <= (backButtonY + 20 + touchMargin)) {
          Serial.println("✅ Zurück-Button erkannt - zurück zum Home-Screen");
          appState.currentMode = HOME_SCREEN;
          appState.renderManager.markFullRedrawRequired();
          break; // Wichtig: Weitere Touch-Verarbeitung überspringen
        }
      }

      // Prüfe Settings-Screen Kalibrierungs-Buttons
      if (appState.currentMode == SETTINGS_SCREEN) {
        if (touchManager.isCalibrating()) {
          // Beenden-Button während Kalibrierung
          int stopButtonX = 170 + appState.antiBurnin.getOffsetX();
          int stopButtonY = 90;
          int stopButtonW = 100;
          int stopButtonH = 30;

          if (event.point.x >= stopButtonX && event.point.x <= stopButtonX + stopButtonW &&
              event.point.y >= stopButtonY && event.point.y <= stopButtonY + stopButtonH) {
            touchManager.stopCalibration();
            appState.renderManager.markFullRedrawRequired();
            break;
          }
        } else {
          // Start-Button wenn nicht kalibriert
          int calibButtonX = 10 + appState.antiBurnin.getOffsetX();
          int calibButtonY = 90;
          int calibButtonW = 150;
          int calibButtonH = 30;

          if (event.point.x >= calibButtonX && event.point.x <= calibButtonX + calibButtonW &&
              event.point.y >= calibButtonY && event.point.y <= calibButtonY + calibButtonH) {
            touchManager.startCalibration();
            appState.renderManager.markFullRedrawRequired();
            break;
          }
        }
      }

      // SmartPlug-Button und Uhrzeit-Touch prüfen (nur auf Home-Screen)
      if (appState.currentMode == HOME_SCREEN) {
        int ox = appState.antiBurnin.getOffsetX();
        int oy = appState.antiBurnin.getOffsetY();

        // Uhrzeit-Bereich (Zone A links, y=0..27) → Settings
        if (event.point.x <= 160 + ox && event.point.y <= 27 + oy) {
          appState.currentMode = SETTINGS_SCREEN;
          appState.lastViewChangeTime = millis();
          appState.renderManager.markFullRedrawRequired();
          break;
        }

        // Plug1-Button in Zone B rechts → nur EINschalten. Ist Plug1 an, passiert
        // bewusst nichts: Ausschalten vom Home-Screen ist gesperrt (Printserver).
        {
          using namespace Plug1ButtonLayout;
          if (event.point.x >= BTN_X + ox && event.point.x <= BTN_X + ox + BTN_W &&
              event.point.y >= BTN_Y + oy && event.point.y <= BTN_Y + oy + BTN_H) {
            SmartPlugState& plug1 = appState.smartPlugs[0];
            if (plug1.isOn) {
              Serial.println("Plug1 ist an - Ausschalten vom Home-Screen gesperrt");
            } else {
              publishPlugCommand(0, true);
              plug1.onRequestedAt = millis();
              appState.renderManager.markFullRedrawRequired();
            }
            break;
          }
        }
      }

#if FEATURE_DEVICES_SCREEN
      // Geraete-Toggle-Buttons (nur auf Geraete-Screen)
      if (appState.currentMode == DEVICES_SCREEN) {
        int ox = appState.antiBurnin.getOffsetX();
        int oy = appState.antiBurnin.getOffsetY();
        using namespace DevicesScreenLayout;

        for (int i = 0; i < SMART_PLUG_COUNT; i++) {
          int btnX = BTN_X + ox;
          int btnY = BTN_Y_START + i * BTN_SPACING + oy;
          if (event.point.x >= btnX && event.point.x <= btnX + BTN_W &&
              event.point.y >= btnY && event.point.y <= btnY + BTN_H) {
            publishPlugCommand(i, !appState.smartPlugs[i].isOn);
            appState.renderManager.markFullRedrawRequired();
            break;
          }
        }
      }
#endif

      // Normale Sensor-Touch-Verarbeitung (nur auf Home-Screen)
      if (event.sensorIndex >= 0 && appState.currentMode == HOME_SCREEN) {
        Serial.printf("Sensor Touch: Index=%d bei (%d,%d)\n",
                     event.sensorIndex, event.point.x, event.point.y);

        switch (event.sensorIndex) {
          case 0: // Ökostrom → Detail
            appState.currentMode = OEKOSTROM_DETAIL_SCREEN;
            appState.lastViewChangeTime = millis();
            appState.renderManager.markFullRedrawRequired();
            break;

          case 1: // Preis → Day-Ahead (bestehender Screen bleibt erhalten)
            appState.currentMode = DAYAHEAD_SCREEN;
            appState.lastViewChangeTime = millis();
            appState.renderManager.markFullRedrawRequired();
            break;

          case 3: // Speicher → Hausspeicher-Detail
            appState.currentMode = LADESTAND_SCREEN;
            appState.lastViewChangeTime = millis();
            appState.renderManager.markFullRedrawRequired();
            break;

          case 4: // Verbrauch (Zone C Platzhalter) → Wallbox-Screen
            appState.currentMode = WALLBOX_CONSUMPTION_SCREEN;
            appState.lastViewChangeTime = millis();
            appState.renderManager.markFullRedrawRequired();
            break;
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
    if (appState.antiBurnin.hasOffsetChanged()) {
      touchManager.updateSensorTouchAreas();
      lastTouchAreaUpdate = millis();
    }
  }
}

