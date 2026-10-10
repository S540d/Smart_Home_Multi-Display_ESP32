#include "network.h"
#include "display.h"
#include "sensors.h"
#include <esp_task_wdt.h>
static SystemState& appState = SystemState::getInstance();


// ═══════════════════════════════════════════════════════════════════════════════
//                              WIFI-MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void connectWiFi() {
  // Sicherheitsprüfung für WiFi-Credentials
  if (!NetworkConfig::WIFI_SSID || strlen(NetworkConfig::WIFI_SSID) == 0) {
    Serial.println("❌ FEHLER: WiFi SSID nicht konfiguriert!");
    Serial.println("   System läuft ohne WiFi weiter...");
    appState.systemStatus.wifiConnected = false;
    return;
  }
  
  if (!NetworkConfig::WIFI_PASSWORD || strlen(NetworkConfig::WIFI_PASSWORD) == 0) {
    Serial.println("❌ FEHLER: WiFi Passwort nicht konfiguriert!");
    Serial.println("   System läuft ohne WiFi weiter...");
    appState.systemStatus.wifiConnected = false;
    return;
  }
  
  Serial.printf("Verbinde mit WiFi '%s'...\n", NetworkConfig::WIFI_SSID);

  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.begin(NetworkConfig::WIFI_SSID, NetworkConfig::WIFI_PASSWORD);

  int attempts = 0;
  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    yield();
    esp_task_wdt_reset();
    Serial.print(".");
    attempts++;

    if (attempts % 10 == 0) {
      Serial.printf("\n🔄 WiFi-Versuch %d...\n", attempts);
    }

    if (millis() - startTime > (Timing::WIFI_CONNECT_TIMEOUT_S * 1000UL)) {
      Serial.println("\nWARNUNG - WiFi-Timeout erreicht!");
      break;
    }
  }

  appState.systemStatus.wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (appState.systemStatus.wifiConnected) {
    appState.systemStatus.wifiRSSI = WiFi.RSSI();
    Serial.printf("\nWiFi verbunden! IP: %s, RSSI: %d dBm\n",
                 WiFi.localIP().toString().c_str(), appState.systemStatus.wifiRSSI);
  } else {
    Serial.println("\n❌ WiFi-Verbindung fehlgeschlagen!");
    appState.systemStatus.hasNetworkError = true;
  }
  
  appState.renderManager.markNetworkStatusChanged();
}

void reconnectWiFi() {
  unsigned long now = millis();
  if (now - appState.lastWifiReconnect >= Timing::WIFI_RECONNECT_INTERVAL) {
    appState.lastWifiReconnect = now;
    Serial.println("🔄 WiFi-Reconnect...");
    connectWiFi();
  }
}

bool checkWifiConnection() {
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (appState.systemStatus.wifiConnected != connected) {
    appState.systemStatus.wifiConnected = connected;
    appState.renderManager.markNetworkStatusChanged();
    
    if (!connected) {
      Serial.println("WiFi-Verbindung verloren!");
      appState.systemStatus.hasNetworkError = true;
    } else {
      Serial.println("WiFi-Verbindung wiederhergestellt!");
      appState.systemStatus.hasNetworkError = false;
      appState.systemStatus.wifiRSSI = WiFi.RSSI();
    }
  }
  
  return connected;
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              MQTT-MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void reconnectMQTT() {
  static unsigned long lastAttempt = 0;
  unsigned long now = millis();
  
  if (now - lastAttempt < Timing::MQTT_RECONNECT_INTERVAL) return;
  lastAttempt = now;
  
  if (!appState.systemStatus.wifiConnected) {
    appState.systemStatus.mqttConnected = false;
    return;
  }
  
  Serial.printf("🔄 MQTT-Verbindung (Versuch #%lu)...", ++appState.systemStatus.mqttReconnectAttempts);
  
  char clientId[64];
  snprintf(clientId, sizeof(clientId), "ESP32Display-%04X-%lu", random(0xffff), appState.systemStatus.mqttReconnectAttempts);
  unsigned long connectStart = millis();
  bool connected = client.connect(clientId, NetworkConfig::MQTT_USER, NetworkConfig::MQTT_PASS);
  
  if (connected) {
    appState.systemStatus.mqttConnected = true;
    Serial.printf(" Verbunden in %lums\n", millis() - connectStart);
    
    // Topics individual abonnieren (sicherer)
    int successCount = 0;
    
    // Standard-Sensor Topics (ohne leere)
    for (int i = 0; i < System::SENSOR_COUNT; i++) {
      if (strlen(NetworkConfig::TOPIC_DATA[i]) > 0) {
        if (client.subscribe(NetworkConfig::TOPIC_DATA[i])) {
          successCount++;
          Serial.printf("✓ Subscribed: %s\n", NetworkConfig::TOPIC_DATA[i]);
        } else {
          Serial.printf("✗ Failed: %s\n", NetworkConfig::TOPIC_DATA[i]);
        }
      }
    }
    
    // Spezial-Topics
    const char* specialTopics[] = {
      NetworkConfig::STOCK_REFERENCE,
      NetworkConfig::STOCK_PREVIOUS_CLOSE,
      NetworkConfig::HISTORY_RESPONSE,
      NetworkConfig::PV_POWER,
      NetworkConfig::GRID_POWER,
      NetworkConfig::LOAD_POWER,
      NetworkConfig::STORAGE_POWER,
      NetworkConfig::WALLBOX_POWER,
      NetworkConfig::ENERGY_MARKET_PRICE_DAY_AHEAD,
      NetworkConfig::CALENDAR_NEXT,
      NetworkConfig::CAR_BATTERY_LEVEL,
      NetworkConfig::CAR_STATE,
      NetworkConfig::GARAGE_DOOR_STATE,
      NetworkConfig::AIR_ECO2
    };

    for (size_t i = 0; i < sizeof(specialTopics) / sizeof(specialTopics[0]); i++) {
      if (client.subscribe(specialTopics[i])) {
        successCount++;
        Serial.printf("✓ Special: %s\n", specialTopics[i]);
        // Debug: Spezielle Meldung für Day-Ahead Topic
        if (strcmp(specialTopics[i], NetworkConfig::ENERGY_MARKET_PRICE_DAY_AHEAD) == 0) {
          Serial.printf("   📊 Day-Ahead Topic erfolgreich abonniert!\n");
        }
      } else {
        Serial.printf("✗ Special Failed: %s\n", specialTopics[i]);
      }
    }
    
    // SmartPlug-State-Topics (Phase 4)
    const char* plugStateTopics[SMART_PLUG_COUNT] = {
      NetworkConfig::PLUG1_STATE, NetworkConfig::PLUG2_STATE
    };
    for (int p = 0; p < SMART_PLUG_COUNT; p++) {
      if (client.subscribe(plugStateTopics[p])) {
        successCount++;
        Serial.printf("✓ Plug%d: %s\n", p + 1, plugStateTopics[p]);
      }
    }

    Serial.printf("📝 %d Topics erfolgreich abonniert\n", successCount);
    appState.renderManager.markNetworkStatusChanged();
    
  } else {
    appState.systemStatus.mqttConnected = false;
    Serial.printf(" ❌ Fehler (Code: %d)\n", client.state());
    
    // Detailliertes Error-Logging
    switch (client.state()) {
      case MQTT_CONNECTION_TIMEOUT:
        Serial.println("   → Timeout beim Verbinden");
        break;
      case MQTT_CONNECTION_LOST:
        Serial.println("   → Verbindung verloren");
        break;
      case MQTT_CONNECT_FAILED:
        Serial.println("   → Verbindung fehlgeschlagen");
        break;
      case MQTT_DISCONNECTED:
        Serial.println("   → Getrennt");
        break;
      case MQTT_CONNECT_BAD_PROTOCOL:
        Serial.println("   → Falsches Protokoll");
        break;
      case MQTT_CONNECT_BAD_CLIENT_ID:
        Serial.println("   → Ungültige Client-ID");
        break;
      case MQTT_CONNECT_UNAVAILABLE:
        Serial.println("   → Server nicht verfügbar");
        break;
      case MQTT_CONNECT_BAD_CREDENTIALS:
        Serial.println("   → Falsche Zugangsdaten");
        break;
      case MQTT_CONNECT_UNAUTHORIZED:
        Serial.println("   → Nicht autorisiert");
        break;
      default:
        Serial.printf("   → Unbekannter Fehler: %d\n", client.state());
    }
    
    appState.systemStatus.hasNetworkError = true;
  }
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  // Day-Ahead JSON kann mehrere Hundert Bytes groß sein → static statt Stack
  static char message[2048];
  if (length >= sizeof(message)) {
    length = sizeof(message) - 1;
  }
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.printf("MQTT: %s = %.40s%s\n", topic, message, length > 40 ? "..." : "");

  // MQTT-Eingang zählt als Aktivität → CPU-Frequenz hochsetzen
  notifyActivity();

  processMqttMessage(topic, String(message));
}

void processMqttMessage(const char* topic, const String& message) {
  // Protect shared state — called from networkTask, read by displayTask.
  if (appState.stateMutex &&
      xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("WARNUNG: MQTT-Mutex Timeout - Nachricht verworfen");
    return;
  }

  // Standard Sensor-Daten verarbeiten (außer PV/Netz und Eco-Score)
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    if (strlen(NetworkConfig::TOPIC_DATA[i]) > 0 && strcmp(topic, NetworkConfig::TOPIC_DATA[i]) == 0) {
      float val = message.toFloat();
      // Preis kommt in ct/kWh direkt an (nicht in €/kWh!)
      if (i == 1) {
        Serial.printf("DEBUG EnergyPrice: raw='%s' parsed=%.2f ct/kWh\n", message.c_str(), val);
      }
      updateSensorValue(i, val);
      if (appState.stateMutex) xSemaphoreGiveRecursive(appState.stateMutex);
      return;
    }
  }

  // Spezielle Topics verarbeiten
  if (strcmp(topic, NetworkConfig::STOCK_REFERENCE) == 0) {
    float newRef = message.toFloat();
    if (appState.stockReference != newRef) {
      appState.stockReference = newRef;
      Serial.printf("📈 Aktien-Referenz aktualisiert: %.2f€\n", appState.stockReference);
      // Aktienkurs neu bewerten falls online
      if (!appState.sensors[2].isTimedOut) {
        appState.renderManager.markSensorChanged(2);
      }
    }
  } else if (strcmp(topic, NetworkConfig::STOCK_PREVIOUS_CLOSE) == 0) {
    float newPrevClose = message.toFloat();
    if (appState.stockPreviousClose != newPrevClose) {
      appState.stockPreviousClose = newPrevClose;
      Serial.printf("Aktien-Vortagespreis aktualisiert: %.2f€\n", appState.stockPreviousClose);
      // Aktienkurs neu bewerten falls online (für Prozentanzeige)
      if (!appState.sensors[2].isTimedOut) {
        appState.renderManager.markSensorChanged(2);
      }
    }
    
  // Power Management Topics
  } else if (strcmp(topic, NetworkConfig::PV_POWER) == 0) {
    float newPVPower = message.toFloat();
    if (appState.pvPower != newPVPower) {
      appState.pvPower = newPVPower;
      // Speichere als letzten gültigen Wert wenn plausibel
      if (appState.pvPower >= 0.0f && appState.pvPower < 30.0f) {
        appState.lastValidPower.pvPower = appState.pvPower;
        appState.lastValidPower.pvPowerTime = millis();
      }
      appState.sensors[5].value = appState.pvPower;
      appState.sensors[5].lastUpdate = millis();
      appState.sensors[5].isTimedOut = false;
      Serial.printf("PV-Erzeugung: %.1fkW\n", appState.pvPower);
      updatePVNetDisplay();
    }

  } else if (strcmp(topic, NetworkConfig::GRID_POWER) == 0) {
    float newGridPower = abs(message.toFloat()); // Immer positiver Wert
    if (appState.gridPower != newGridPower) {
      appState.gridPower = newGridPower;
      // Speichere als letzten gültigen Wert wenn plausibel
      if (appState.gridPower >= 0.0f && appState.gridPower < 50.0f) {
        appState.lastValidPower.gridPower = appState.gridPower;
        appState.lastValidPower.gridPowerTime = millis();
      }
      Serial.printf("Netz-Leistung: %.1fkW (Richtung wird berechnet)\n", appState.gridPower);
      updatePVNetDisplay();
    }

  } else if (strcmp(topic, NetworkConfig::LOAD_POWER) == 0) {
    float newLoadPower = message.toFloat();
    if (appState.loadPower != newLoadPower) {
      appState.loadPower = newLoadPower;
      // Speichere als letzten gültigen Wert wenn plausibel
      if (appState.loadPower >= 0.0f && appState.loadPower < 50.0f) {
        appState.lastValidPower.loadPower = appState.loadPower;
        appState.lastValidPower.loadPowerTime = millis();
      }
      Serial.printf("Hausverbrauch: %.1fkW\n", appState.loadPower);
      updatePVNetDisplay();
    }

  } else if (strcmp(topic, NetworkConfig::STORAGE_POWER) == 0) {
    float newStoragePower = abs(message.toFloat()); // Immer positiver Wert
    if (appState.storagePower != newStoragePower) {
      appState.storagePower = newStoragePower;
      // Speichere als letzten gültigen Wert wenn plausibel
      if (appState.storagePower >= 0.0f && appState.storagePower < 20.0f) {
        appState.lastValidPower.storagePower = appState.storagePower;
        appState.lastValidPower.storagePowerTime = millis();
      }
      Serial.printf("Speicher-Leistung: %.1fkW (Richtung wird berechnet)\n", appState.storagePower);
      updatePVNetDisplay();
    }

  } else if (strcmp(topic, NetworkConfig::WALLBOX_POWER) == 0) {
    // home/PV/WallboxPower liefert die Ladeleistung in WATT, appState.wallboxPower
    // ist wie alle anderen PowerManagement-Werte in kW - Umrechnung hier, einmalig.
    // (Das war der ursprüngliche Bug: der Watt-Wert wurde ungerechnet als kW angezeigt.)
    float newWallboxPower = abs(message.toFloat()) / 1000.0f;
    // Unplausible Werte (defekte/fehlerhafte MQTT-Quelle, z.B. Millionen statt kW)
    // gar nicht erst übernehmen - sonst zeigt das Display absurde Zahlen an.
    if (newWallboxPower >= 30.0f) { // Max 30kW für Wallbox
      Serial.printf("⚠️ Wallbox-Leistung verworfen (unplausibel): %.1f\n", newWallboxPower);
    } else {
      // Zeitstempel bei JEDER gueltigen Nachricht (auch unveraendert) - Basis fuer
      // die Verbindungsanzeige auf dem Wallbox-Screen.
      appState.lastValidPower.wallboxPowerTime = millis();
      if (appState.wallboxPower != newWallboxPower) {
        appState.wallboxPower = newWallboxPower;
        appState.lastValidPower.wallboxPower = appState.wallboxPower;
        Serial.printf("Wallbox-Leistung: %.1fkW\n", appState.wallboxPower);
        // Markiere PV-Sensor für Update (Index 5) da die PV-Distribution davon abhängt
        appState.renderManager.markSensorChanged(5);
      }
    }

  // Calendar functionality removed
    
  } else if (strcmp(topic, NetworkConfig::HISTORY_RESPONSE) == 0) {
    Serial.printf("History-Response: %s\n", message.c_str());
    // Note: History screen feature to be implemented in future version

  } else if (strcmp(topic, NetworkConfig::ENERGY_MARKET_PRICE_DAY_AHEAD) == 0) {
    processDayAheadPriceData(message);

  } else if (strcmp(topic, NetworkConfig::CALENDAR_NEXT) == 0) {
    strncpy(appState.calendar.nextEventText, message.c_str(), sizeof(appState.calendar.nextEventText) - 1);
    appState.calendar.nextEventText[sizeof(appState.calendar.nextEventText) - 1] = '\0';
    appState.calendar.hasEvent = message.length() > 0;
    appState.calendar.lastUpdate = millis();
    appState.calendar.staleShown = appState.calendar.isStale();
    Serial.printf("Kalender: %s\n", appState.calendar.nextEventText);
    appState.renderManager.markSystemInfoChanged(); // Fußzeilen-Bereich neu zeichnen

  } else if (strcmp(topic, NetworkConfig::CAR_BATTERY_LEVEL) == 0) {
    float level = message.toFloat();
    if (message.length() > 0 && level >= 0.0f && level <= 100.0f) {
      appState.car.batteryLevel = level;
      appState.car.hasValue = true;
      appState.car.lastUpdate = millis();
      appState.car.staleShown = false;
      Serial.printf("PKW-Ladestand: %.0f%%\n", level);
      // PKW-Zeile liegt in der Speicher-Box (Zone D rechts, sensors[3])
      appState.renderManager.markSensorChanged(3);
    } else {
      Serial.printf("⚠️ PKW-Ladestand verworfen (unplausibel): %s\n", message.c_str());
    }

  } else if (strcmp(topic, NetworkConfig::CAR_STATE) == 0) {
    strncpy(appState.car.state, message.c_str(), sizeof(appState.car.state) - 1);
    appState.car.state[sizeof(appState.car.state) - 1] = '\0';
    Serial.printf("PKW-Status: %s\n", appState.car.state);
    appState.renderManager.markSensorChanged(3);

  } else if (strcmp(topic, NetworkConfig::GARAGE_DOOR_STATE) == 0) {
    bool nowOpen = (message == "open");
    // Nur bei einem Wechsel reagieren: Node-RED sendet den retained Wert bei jeder
    // Messung erneut, das darf eine quittierte Anzeige nicht wieder einblenden.
    if (nowOpen != appState.garage.isOpen) {
      appState.garage.isOpen = nowOpen;
      appState.garage.dismissed = false;
      appState.garage.alertDrawn = false;
      Serial.printf("Garagentor: %s\n", nowOpen ? "OFFEN" : "zu");
      appState.renderManager.markFullRedrawRequired();
    }

  } else if (strcmp(topic, NetworkConfig::AIR_ECO2) == 0) {
    int ppm = message.toInt();
    if (ppm <= 0) return;  // ungültig / SGP30-Fehler
    appState.air.eco2 = ppm;
    bool was = appState.air.ventilate;
    if (!was && ppm > AirQualityState::WARN_PPM) {
      appState.air.ventilate = true;
      appState.air.dismissed = false;
      appState.air.alertDrawn = false;
      Serial.printf("Luftqualität: LÜFTEN (%d ppm)\n", ppm);
      appState.renderManager.markFullRedrawRequired();
    } else if (was && ppm <= AirQualityState::CLEAR_PPM) {
      appState.air.ventilate = false;
      appState.air.dismissed = false;
      appState.air.alertDrawn = false;
      Serial.printf("Luftqualität: wieder gut (%d ppm)\n", ppm);
      appState.renderManager.markFullRedrawRequired();
    }

  } else {
    // SmartPlug-State-Topics (Phase 4)
    const char* plugStateTopics[SMART_PLUG_COUNT] = {
      NetworkConfig::PLUG1_STATE, NetworkConfig::PLUG2_STATE
    };
    bool handledPlug = false;
    for (int p = 0; p < SMART_PLUG_COUNT; p++) {
      if (strcmp(topic, plugStateTopics[p]) == 0) {
        bool newState = (message == "ON" || message == "1" || message == "true");
        SmartPlugState& plug = appState.smartPlugs[p];
        // Retained/periodische Wiederholungen ohne Aenderung loesen keinen Redraw aus
        bool changed = (plug.isOn != newState) || plug.isTimedOut;
        plug.isOn = newState;
        plug.isTimedOut = false;
        plug.lastUpdate = millis();
        if (newState) plug.onRequestedAt = 0;  // Rueckmeldung da -> "schaltet" beenden
        Serial.printf("SmartPlug %d: %s\n", p + 1, newState ? "EIN" : "AUS");
        // Plug-Button liegt in Zone B (drawHomeScreen), nicht in Zone E — es gibt
        // keinen granularen Redraw-Flag dafür, daher voller Redraw nötig.
        if (changed) appState.renderManager.markFullRedrawRequired();
        handledPlug = true;
        break;
      }
    }
    if (!handledPlug) {
      Serial.printf("WARNUNG - Unbekanntes MQTT-Topic: %s\n", topic);
    }
  }

  if (appState.stateMutex) xSemaphoreGiveRecursive(appState.stateMutex);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              SENSOR-DATENVERARBEITUNG
// ═══════════════════════════════════════════════════════════════════════════════

void updateSensorValue(int index, float newValue) {
  if (!isValidSensorIndex(index)) {
    Serial.printf("WARNUNG - Ungültiger Sensor-Index: %d\n", index);
    return;
  }
  
  SensorData& sensor = appState.sensors[index];
  
  // Timeout zurücksetzen bei neuen Daten
  sensor.lastUpdate = millis();
  bool wasTimedOut = sensor.isTimedOut;
  sensor.isTimedOut = false;
  
  // Trend-Analyse (nur bei gültigen vorherigen Werten)
  if (!wasTimedOut && sensor.value != newValue) {
    sensor.lastValue = sensor.value;
    
    if (newValue > sensor.value) {
      sensor.trend = SensorData::UP;
    } else if (newValue < sensor.value) {
      sensor.trend = SensorData::DOWN;
    } else {
      sensor.trend = SensorData::STABLE;
    }
  }
  
  validateSensorRange(index, newValue);

  // Wert aktualisieren und formatieren
  sensor.value = newValue;
  sensor.formatValue();
  sensor.hasChanged = true;
  
  // Status-Änderung loggen
  if (wasTimedOut) {
    Serial.printf("🔄 Sensor %d (%s) wieder online: %s\n", 
                 index, sensor.label, sensor.formattedValue);
  } else {
    Serial.printf("%s: %s (Trend: %s)\n", 
                 sensor.label, sensor.formattedValue,
                 sensor.trend == SensorData::UP ? "↗" : 
                 sensor.trend == SensorData::DOWN ? "↘" : "→");
  }
  
  // Render-System benachrichtigen
  appState.renderManager.markSensorChanged(index);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              HILFSFUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

bool isValidSensorIndex(int index) {
  return index >= 0 && index < System::SENSOR_COUNT;
}

void calculatePowerDirections() {
  // Berechne Energiebilanz: PV-Erzeugung minus Hausverbrauch
  float energyBalance = appState.pvPower - appState.loadPower;

  // Grid-Richtung bestimmen
  if (energyBalance > 0) {
    // PV-Überschuss: Normalerweise Einspeisung (außer Speicher lädt alles)
    appState.isGridFeedIn = true;  // Einspeisung
  } else {
    // PV-Defizit: Normalerweise Netzbezug
    appState.isGridFeedIn = false; // Bezug
  }

  // Speicher-Richtung: Heuristik basiert auf Energiebilanz
  if (energyBalance > 1.0f) {
    // Deutlicher PV-Überschuss: Speicher lädt wahrscheinlich
    appState.isStorageCharging = true;
  } else if (energyBalance < -1.0f) {
    // Deutliches PV-Defizit: Speicher entlädt wahrscheinlich
    appState.isStorageCharging = false;
  }
  // Bei geringem Unterschied: Richtung unverändert lassen

  Serial.printf("Richtung: Grid=%s (%.1fkW), Storage=%s (%.1fkW), Balance=%.1fkW\n",
                appState.isGridFeedIn ? "Feed" : "Bezug", appState.gridPower,
                appState.isStorageCharging ? "Laden" : "Entladen", appState.storagePower, energyBalance);
}



void updatePVNetDisplay() {
  // Sensor Index 4 für PV/Netz Display aktualisieren
  if (!isValidSensorIndex(4)) return;

  // Verwende Mischung aus aktuellen und letzten gültigen Werten für stabile Berechnung
  float workingPvPower = appState.pvPower;
  float workingGridPower = appState.gridPower;
  float workingLoadPower = appState.loadPower;
  float workingStoragePower = appState.storagePower;
  float workingWallboxPower = appState.wallboxPower;

  unsigned long now = millis();
  const unsigned long MAX_AGE_MS = 300000; // 5 Minuten maximales Alter

  // Verwende letzte gültige Werte wenn aktuelle Werte ungültig oder zu alt sind
  if ((appState.pvPower <= 0.0f || appState.pvPower >= 30.0f) &&
      (now - appState.lastValidPower.pvPowerTime) < MAX_AGE_MS &&
      appState.lastValidPower.pvPower > 0.0f) {
    workingPvPower = appState.lastValidPower.pvPower;
    Serial.printf("📅 Verwende letzten PV-Wert: %.1fkW\n", workingPvPower);
  }

  if ((appState.gridPower < 0.0f || appState.gridPower >= 50.0f) &&
      (now - appState.lastValidPower.gridPowerTime) < MAX_AGE_MS &&
      appState.lastValidPower.gridPower >= 0.0f) {
    workingGridPower = appState.lastValidPower.gridPower;
    Serial.printf("📅 Verwende letzten Grid-Wert: %.1fkW\n", workingGridPower);
  }

  if ((appState.loadPower <= 0.0f || appState.loadPower >= 50.0f) &&
      (now - appState.lastValidPower.loadPowerTime) < MAX_AGE_MS &&
      appState.lastValidPower.loadPower > 0.0f) {
    workingLoadPower = appState.lastValidPower.loadPower;
    Serial.printf("📅 Verwende letzten Load-Wert: %.1fkW\n", workingLoadPower);
  }

  if ((appState.storagePower < 0.0f || appState.storagePower >= 20.0f) &&
      (now - appState.lastValidPower.storagePowerTime) < MAX_AGE_MS &&
      appState.lastValidPower.storagePower >= 0.0f) {
    workingStoragePower = appState.lastValidPower.storagePower;
    Serial.printf("📅 Verwende letzten Storage-Wert: %.1fkW\n", workingStoragePower);
  }

  // Wallbox Power Fallback
  if ((appState.wallboxPower < 0.0f || appState.wallboxPower >= 30.0f) &&
      (now - appState.lastValidPower.wallboxPowerTime) < MAX_AGE_MS &&
      appState.lastValidPower.wallboxPower >= 0.0f) {
    workingWallboxPower = appState.lastValidPower.wallboxPower;
    Serial.printf("📅 Verwende letzten Wallbox-Wert: %.1fkW\n", workingWallboxPower);
  }

  // Robuste Power-Validierung mit Working-Werten
  bool hasValidData = false;

  // Prüfe ob wir mindestens einen plausiblen Wert haben
  if (workingLoadPower > 0.0f && workingLoadPower < 50.0f) {
    hasValidData = true;
  } else if (workingPvPower > 0.0f && workingPvPower < 30.0f) {
    hasValidData = true;
    // Schätze Verbrauch aus PV wenn appState.loadPower nicht verfügbar
    if (workingLoadPower <= 0.0f || workingLoadPower >= 50.0f) {
      workingLoadPower = max(0.1f, workingPvPower * 0.8f);
      Serial.printf("💡 Verbrauch geschätzt aus PV: %.1fkW\n", workingLoadPower);
    }
  }

  if (!hasValidData) {
    // Fallback: Behalte den letzten gültigen Sensor-Wert bei oder setze Default nur einmal
    if (appState.sensors[4].value <= 0.0f && appState.sensors[4].isTimedOut) {
      appState.sensors[4].value = 2.0f; // Default: 2kW Grundverbrauch, nur wenn noch nie gesetzt
      appState.sensors[4].formattedValue[0] = '\0'; // Force reformatting
      appState.sensors[4].hasChanged = true;
      Serial.println("⚠️ Fallback: Standard-Verbrauch 2kW verwendet (einmalig)");
    }
    // Sensor als nicht-timeout markieren um Display beizubehalten
    appState.sensors[4].isTimedOut = false;
    appState.sensors[4].lastUpdate = millis();
    appState.sensors[4].formatValue();
    appState.renderManager.markSensorChanged(4);
    Serial.printf("🔄 Behalte letzten Verbrauchswert bei: %.1fkW\n", appState.sensors[4].value);
    return;
  }

  // Temporär die Working-Werte in die globalen Variablen kopieren für calculatePowerDirections()
  float originalPvPower = appState.pvPower;
  float originalGridPower = appState.gridPower;
  float originalLoadPower = appState.loadPower;
  float originalStoragePower = appState.storagePower;
  float originalWallboxPower = appState.wallboxPower;

  appState.pvPower = workingPvPower;
  appState.gridPower = workingGridPower;
  appState.loadPower = workingLoadPower;
  appState.storagePower = workingStoragePower;
  appState.wallboxPower = workingWallboxPower;
  
  // Richtungen berechnen
  calculatePowerDirections();

  // Sensor-Daten aktualisieren - jetzt mit validiertem Working-Verbrauch
  appState.sensors[4].isTimedOut = false;
  appState.sensors[4].lastUpdate = millis();
  appState.sensors[4].value = workingLoadPower;  // Verwende den validierten Working-Wert

  // Originale Werte wiederherstellen
  appState.pvPower = originalPvPower;
  appState.gridPower = originalGridPower;
  appState.loadPower = originalLoadPower;
  appState.storagePower = originalStoragePower;
  appState.wallboxPower = originalWallboxPower;

  // Sensor als geändert markieren
  appState.sensors[4].hasChanged = true;
  appState.sensors[4].requiresRedraw = true;
  appState.sensors[4].formatValue();  // Wert formatieren
  appState.renderManager.markSensorChanged(4);
  
  
  Serial.printf("✅ PV/Grid Update: Verbrauch %.1fkW (stabil mit Mix aus neuen/alten Werten)\n",
                workingLoadPower);
}

void processDayAheadPriceData(const String& message) {
  Serial.printf("📊 Day-Ahead Preisdaten empfangen (%d Zeichen)\n", message.length());

  // Neues Format: [{"t":timestamp,"v":value},{"t":timestamp,"v":value},...]

  appState.dayAheadPrices.clear();

  // Aktuelles Datum setzen (da nicht im JSON enthalten)
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  strftime(appState.dayAheadPrices.date, sizeof(appState.dayAheadPrices.date), "%d.%m.%Y", timeinfo);

  Serial.printf("   Datum gesetzt auf: %s\n", appState.dayAheadPrices.date);

  // Parse neues Format: [{"h":hour,"v":value},...]
  if (message.startsWith("[") && message.endsWith("]")) {
    int currentPos = 1; // Nach "["
    int entryCount = 0;

    while (currentPos < message.length() - 1 && entryCount < 24) {
      // Suche nach {"h":
      int hStart = message.indexOf("\"h\":", currentPos);
      if (hStart == -1) break;
      hStart += 4;

      // Suche nach Stunden-Wert
      int hEnd = message.indexOf(",", hStart);
      if (hEnd == -1) break;

      int hour = message.substring(hStart, hEnd).toInt();

      // Suche nach "v":
      int vStart = message.indexOf("\"v\":", hEnd);
      if (vStart == -1) break;
      vStart += 4;

      // Suche nach Preis-Wert
      int vEnd = message.indexOf("}", vStart);
      if (vEnd == -1) break;

      float value = message.substring(vStart, vEnd).toFloat();

      // Debug: Zeige erste 5 Einträge
      if (entryCount < 5) {
        Serial.printf("🔍 Eintrag %d: Stunde=%d, Wert=%.2f\n", entryCount, hour, value);
      }

      // Speichere in appState.dayAheadPrices (verwende hour direkt als Index)
      if (hour >= 0 && hour < 24) {
        char hourStr[6];
        snprintf(hourStr, sizeof(hourStr), "%02d:00", hour);

        appState.dayAheadPrices.prices[hour].price = value;
        strncpy(appState.dayAheadPrices.prices[hour].hour, hourStr,
                sizeof(appState.dayAheadPrices.prices[hour].hour) - 1);
        appState.dayAheadPrices.prices[hour].hour[sizeof(appState.dayAheadPrices.prices[hour].hour) - 1] = '\0';
        appState.dayAheadPrices.prices[hour].isValid = true;

        Serial.printf("   Stunde %02d:00: %.2f ct/kWh\n", hour, value);
        entryCount++;
      }

      // Nächstes Element
      currentPos = message.indexOf(",{", vEnd);
      if (currentPos == -1) break;
      currentPos += 1; // Nach ","
    }

    appState.dayAheadPrices.hasData = (entryCount > 0);
    appState.dayAheadPrices.lastUpdate = millis();

    // Calculate analytics for enhanced insights
    if (appState.dayAheadPrices.hasData) {
      appState.dayAheadPrices.calculateAnalytics();
      Serial.printf("📊 Analytics: Avg=%.1f¢, Min=%.1f¢@%02d:00, Max=%.1f¢@%02d:00, Quality=%d%%\n",
                    appState.dayAheadPrices.dailyAverage, appState.dayAheadPrices.minPrice, appState.dayAheadPrices.cheapestHour,
                    appState.dayAheadPrices.maxPrice, appState.dayAheadPrices.expensiveHour, appState.dayAheadPrices.dataQuality);

      // Log optimal windows
      for (int i = 0; i < 3; i++) {
        if (appState.dayAheadPrices.optimalWindows[i].isAvailable) {
          Serial.printf("🎯 Optimal Window %d: %02d:00-%02d:00 (Avg: %.1f¢, Save: %.1f¢)\n",
                        i + 1, appState.dayAheadPrices.optimalWindows[i].startHour,
                        appState.dayAheadPrices.optimalWindows[i].endHour,
                        appState.dayAheadPrices.optimalWindows[i].averagePrice,
                        appState.dayAheadPrices.optimalWindows[i].savingsVsPeak);
        }
      }

      // Log trend info
      const char* trendStr = (appState.dayAheadPrices.trend == TREND_RISING) ? "📈 Rising" :
                            (appState.dayAheadPrices.trend == TREND_FALLING) ? "📉 Falling" :
                            "📊 Stable";
      Serial.printf("%s trend, Volatility: %.1f%%, Potential savings: %.1f¢\n",
                    trendStr, appState.dayAheadPrices.volatilityIndex, appState.dayAheadPrices.potentialSavings);
    }

    Serial.printf("✅ Day-Ahead Daten verarbeitet: %d Preise für %s\n",
                  entryCount, appState.dayAheadPrices.date);

    // Aktuellen Preis für Haupt-Display extrahieren und in Sensor[1] setzen
    if (entryCount > 0) {
      // Finde aktuelle Stunde
      time_t nowTime = time(nullptr);
      struct tm* currentTimeInfo = localtime(&nowTime);
      int currentHour = currentTimeInfo->tm_hour;

      // Verwende aktuellen Preis wenn verfügbar, sonst ersten verfügbaren
      float currentDayAheadPrice = 0.0f;
      bool foundCurrentPrice = false;

      if (currentHour < 24 && appState.dayAheadPrices.prices[currentHour].isValid) {
        currentDayAheadPrice = appState.dayAheadPrices.prices[currentHour].price;
        foundCurrentPrice = true;
        Serial.printf("📈 Aktueller Day-Ahead Preis (%02d:00): %.2f ct/kWh\n", currentHour, currentDayAheadPrice);
      } else {
        // Fallback: Finde ersten gültigen Preis
        for (int i = 0; i < 24; i++) {
          if (appState.dayAheadPrices.prices[i].isValid) {
            currentDayAheadPrice = appState.dayAheadPrices.prices[i].price;
            Serial.printf("📈 Day-Ahead Preis (Fallback %02d:00): %.2f ct/kWh\n", i, currentDayAheadPrice);
            break;
          }
        }
      }

      // Update Sensor[1] nur wenn direkte MQTT-Quelle (home/PV/EnergyPrice) älter als 2 Minuten
      if (currentDayAheadPrice > 0.0f) {
        unsigned long directPriceAge = millis() - appState.sensors[1].lastUpdate;
        if (appState.sensors[1].isTimedOut || directPriceAge > 120000UL) {
          updateSensorValue(1, currentDayAheadPrice);
          Serial.printf("Day-Ahead Preis übernommen (direkte Quelle %lus alt): %.2f ct/kWh\n",
                        directPriceAge / 1000, currentDayAheadPrice);
        } else {
          Serial.printf("Day-Ahead Preis ignoriert (direkte Quelle aktuell, %lus alt)\n",
                        directPriceAge / 1000);
        }
      }
    }

    // Rate-Limited Display Update: Nur alle 30 Sekunden, um Touch-Konflikte zu vermeiden
    static unsigned long lastDisplayUpdate = 0;
    unsigned long now = millis();

    if (appState.currentMode == DAYAHEAD_SCREEN &&
        (now - lastDisplayUpdate) > 30000) { // 30 Sekunden Mindestabstand
      appState.renderManager.markFullRedrawRequired();
      lastDisplayUpdate = now;
      Serial.println("🔄 Day-Ahead Display Update (rate-limited)");
    }
  } else {
    Serial.println("❌ Fehler beim Parsen der Day-Ahead Daten - kein prices Array gefunden");
  }
  // Note: mutex is managed by the caller (processMqttMessage)
}