#include "network.h"
#include "display.h"  // Für tft-Zugriff während WiFi-Setup

// ═══════════════════════════════════════════════════════════════════════════════
//                              WIFI-MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void connectWiFi() {
  // Sicherheitsprüfung für WiFi-Credentials
  if (!NetworkConfig::WIFI_SSID || strlen(NetworkConfig::WIFI_SSID) == 0) {
    Serial.println("❌ FEHLER: WiFi SSID nicht konfiguriert!");
    Serial.println("   System läuft ohne WiFi weiter...");
    systemStatus.wifiConnected = false;
    return;
  }
  
  if (!NetworkConfig::WIFI_PASSWORD || strlen(NetworkConfig::WIFI_PASSWORD) == 0) {
    Serial.println("❌ FEHLER: WiFi Passwort nicht konfiguriert!");
    Serial.println("   System läuft ohne WiFi weiter...");
    systemStatus.wifiConnected = false;
    return;
  }
  
  Serial.printf("Verbinde mit WiFi '%s'...\n", NetworkConfig::WIFI_SSID);
  
  // WiFi sicher initialisieren
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Status-Anzeige auf Display (mit try-catch für Sicherheit)
  extern TFT_eSPI tft;
  try {
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("WiFi verbinden...", 10, 50, 1);
  } catch (...) {
    Serial.println("WARNUNG - Display-Zugriff fehlgeschlagen");
  }
  
  WiFi.begin(NetworkConfig::WIFI_SSID, NetworkConfig::WIFI_PASSWORD);
  
  int attempts = 0;
  unsigned long startTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && attempts < Timing::WIFI_CONNECT_TIMEOUT_S) {
    delay(250); // Kürzere Delays um Watchdog zu vermeiden
    yield(); // ESP32-Watchdog füttern
    Serial.print(".");
    attempts++;
    
    // Zusätzlicher Watchdog-Reset Schutz
    if (attempts % 10 == 0) {
      Serial.printf("\n🔄 WiFi-Versuch %d/%d...\n", attempts, Timing::WIFI_CONNECT_TIMEOUT_S);
    }
    
    if (millis() - startTime > (Timing::WIFI_CONNECT_TIMEOUT_S * 1000)) {
      Serial.println("\nWARNUNG - WiFi-Timeout erreicht!");
      break;
    }
  }
  
  systemStatus.wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  if (systemStatus.wifiConnected) {
    systemStatus.wifiRSSI = WiFi.RSSI();
    Serial.printf("\nWiFi verbunden! IP: %s, RSSI: %d dBm\n", 
                 WiFi.localIP().toString().c_str(), systemStatus.wifiRSSI);
    
    tft.setTextColor(Colors::STATUS_GREEN);
    tft.drawString("WiFi: OK", 10, 70, 1);
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString(WiFi.localIP().toString().c_str(), 10, 85, 1);
    
  } else {
    Serial.println("\n❌ WiFi-Verbindung fehlgeschlagen!");
    systemStatus.hasNetworkError = true;
    
    tft.setTextColor(Colors::SYSTEM_ERROR);
    tft.drawString("WiFi: FEHLER", 10, 70, 1);
  }
  
  renderManager.markNetworkStatusChanged();
}

void reconnectWiFi() {
  unsigned long now = millis();
  if (now - lastWifiReconnect >= Timing::WIFI_RECONNECT_INTERVAL) {
    lastWifiReconnect = now;
    Serial.println("🔄 WiFi-Reconnect...");
    connectWiFi();
  }
}

bool checkWifiConnection() {
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (systemStatus.wifiConnected != connected) {
    systemStatus.wifiConnected = connected;
    renderManager.markNetworkStatusChanged();
    
    if (!connected) {
      Serial.println("WiFi-Verbindung verloren!");
      systemStatus.hasNetworkError = true;
    } else {
      Serial.println("WiFi-Verbindung wiederhergestellt!");
      systemStatus.hasNetworkError = false;
      systemStatus.wifiRSSI = WiFi.RSSI();
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
  
  if (!systemStatus.wifiConnected) {
    systemStatus.mqttConnected = false;
    return;
  }
  
  Serial.printf("🔄 MQTT-Verbindung (Versuch #%lu)...", ++systemStatus.mqttReconnectAttempts);
  
  char clientId[64];
  snprintf(clientId, sizeof(clientId), "ESP32Display-%04X-%lu", random(0xffff), systemStatus.mqttReconnectAttempts);

  unsigned long connectStart = millis();
  bool connected = client.connect(clientId, NetworkConfig::MQTT_USER, NetworkConfig::MQTT_PASS);
  
  if (connected) {
    systemStatus.mqttConnected = true;
    Serial.printf(" Verbunden in %lums\n", millis() - connectStart);
    
    // Topics individual abonnieren (sicherer)
    int successCount = 0;
    
    // Standard-Sensor Topics (ohne leere)
    for (int i = 0; i < System::SENSOR_COUNT; i++) {
      if (strlen(NetworkConfig::topics.data[i]) > 0) {
        if (client.subscribe(NetworkConfig::topics.data[i])) {
          successCount++;
          Serial.printf("✓ Subscribed: %s\n", NetworkConfig::topics.data[i]);
        } else {
          Serial.printf("✗ Failed: %s\n", NetworkConfig::topics.data[i]);
        }
      }
    }
    
    // Spezial-Topics
    const char* specialTopics[] = {
      NetworkConfig::topics.stockReference,
      NetworkConfig::topics.stockPreviousClose,
      NetworkConfig::topics.historyResponse,
      NetworkConfig::topics.pvPower,
      NetworkConfig::topics.gridPower,
      NetworkConfig::topics.loadPower,
      NetworkConfig::topics.storagePower,
      NetworkConfig::topics.energyMarketPriceDayAhead
    };

    for (int i = 0; i < 8; i++) {
      if (client.subscribe(specialTopics[i])) {
        successCount++;
        Serial.printf("✓ Special: %s\n", specialTopics[i]);
      } else {
        Serial.printf("✗ Special Failed: %s\n", specialTopics[i]);
      }
    }
    
    Serial.printf("📝 %d Topics erfolgreich abonniert\n", successCount);
    renderManager.markNetworkStatusChanged();
    
  } else {
    systemStatus.mqttConnected = false;
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
    
    systemStatus.hasNetworkError = true;
  }
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  if (length >= System::MAX_STRING_LENGTH) {
    Serial.printf("WARNUNG - MQTT-Message zu lang (%u bytes), kürze auf %d\n", length, System::MAX_STRING_LENGTH - 1);
    length = System::MAX_STRING_LENGTH - 1;
  }
  
  char message[System::MAX_STRING_LENGTH];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.printf("MQTT: %s = %s\n", topic, message);
  
  processMqttMessage(topic, String(message));
}

void processMqttMessage(const char* topic, const String& message) {
  // Standard Sensor-Daten verarbeiten (außer PV/Netz und Eco-Score)
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    if (strlen(NetworkConfig::topics.data[i]) > 0 && strcmp(topic, NetworkConfig::topics.data[i]) == 0) {
      updateSensorValue(i, message.toFloat());
      return;
    }
  }
  
  // Spezielle Topics verarbeiten
  if (strcmp(topic, NetworkConfig::topics.stockReference) == 0) {
    float newRef = message.toFloat();
    if (stockReference != newRef) {
      stockReference = newRef;
      Serial.printf("📈 Aktien-Referenz aktualisiert: %.2f€\n", stockReference);
      // Aktienkurs neu bewerten falls online
      if (!sensors[2].isTimedOut) {
        renderManager.markSensorChanged(2);
      }
    }
  } else if (strcmp(topic, NetworkConfig::topics.stockPreviousClose) == 0) {
    float newPrevClose = message.toFloat();
    if (stockPreviousClose != newPrevClose) {
      stockPreviousClose = newPrevClose;
      Serial.printf("Aktien-Vortagespreis aktualisiert: %.2f€\n", stockPreviousClose);
      // Aktienkurs neu bewerten falls online (für Prozentanzeige)
      if (!sensors[2].isTimedOut) {
        renderManager.markSensorChanged(2);
      }
    }
    
  // Power Management Topics
  } else if (strcmp(topic, NetworkConfig::topics.pvPower) == 0) {
    float newPVPower = message.toFloat();
    if (pvPower != newPVPower) {
      pvPower = newPVPower;
      // Speichere als letzten gültigen Wert wenn plausibel
      if (pvPower >= 0.0f && pvPower < 30.0f) {
        lastValidPower.pvPower = pvPower;
        lastValidPower.pvPowerTime = millis();
      }
      Serial.printf("PV-Erzeugung: %.1fkW\n", pvPower);
      updatePVNetDisplay();
    }

  } else if (strcmp(topic, NetworkConfig::topics.gridPower) == 0) {
    float newGridPower = abs(message.toFloat()); // Immer positiver Wert
    if (gridPower != newGridPower) {
      gridPower = newGridPower;
      // Speichere als letzten gültigen Wert wenn plausibel
      if (gridPower >= 0.0f && gridPower < 50.0f) {
        lastValidPower.gridPower = gridPower;
        lastValidPower.gridPowerTime = millis();
      }
      Serial.printf("Netz-Leistung: %.1fkW (Richtung wird berechnet)\n", gridPower);
      updatePVNetDisplay();
    }

  } else if (strcmp(topic, NetworkConfig::topics.loadPower) == 0) {
    float newLoadPower = message.toFloat();
    if (loadPower != newLoadPower) {
      loadPower = newLoadPower;
      // Speichere als letzten gültigen Wert wenn plausibel
      if (loadPower >= 0.0f && loadPower < 50.0f) {
        lastValidPower.loadPower = loadPower;
        lastValidPower.loadPowerTime = millis();
      }
      Serial.printf("Hausverbrauch: %.1fkW\n", loadPower);
      updatePVNetDisplay();
    }

  } else if (strcmp(topic, NetworkConfig::topics.storagePower) == 0) {
    float newStoragePower = abs(message.toFloat()); // Immer positiver Wert
    if (storagePower != newStoragePower) {
      storagePower = newStoragePower;
      // Speichere als letzten gültigen Wert wenn plausibel
      if (storagePower >= 0.0f && storagePower < 20.0f) {
        lastValidPower.storagePower = storagePower;
        lastValidPower.storagePowerTime = millis();
      }
      Serial.printf("Speicher-Leistung: %.1fkW (Richtung wird berechnet)\n", storagePower);
      updatePVNetDisplay();
    }
    
  // Calendar functionality removed
    
  } else if (strcmp(topic, NetworkConfig::topics.historyResponse) == 0) {
    Serial.printf("History-Response: %s\n", message.c_str());
    // Note: History screen feature to be implemented in future version

  } else if (strcmp(topic, NetworkConfig::topics.energyMarketPriceDayAhead) == 0) {
    processDayAheadPriceData(message);

  } else {
    Serial.printf("WARNUNG - Unbekanntes MQTT-Topic: %s\n", topic);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              SENSOR-DATENVERARBEITUNG
// ═══════════════════════════════════════════════════════════════════════════════

void updateSensorValue(int index, float newValue) {
  if (!isValidSensorIndex(index)) {
    Serial.printf("WARNUNG - Ungültiger Sensor-Index: %d\n", index);
    return;
  }
  
  SensorData& sensor = sensors[index];
  
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
  renderManager.markSensorChanged(index);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              HILFSFUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

bool isValidSensorIndex(int index) {
  return index >= 0 && index < System::SENSOR_COUNT;
}

void calculatePowerDirections() {
  // Berechne Energiebilanz: PV-Erzeugung minus Hausverbrauch
  float energyBalance = pvPower - loadPower;
  
  // Grid-Richtung bestimmen
  if (energyBalance > 0) {
    // PV-Überschuss: Normalerweise Einspeisung (außer Speicher lädt alles)
    isGridFeedIn = true;  // Einspeisung
  } else {
    // PV-Defizit: Normalerweise Netzbezug
    isGridFeedIn = false; // Bezug
  }
  
  // Speicher-Richtung: Heuristik basiert auf Energiebilanz
  if (energyBalance > 1.0f) {
    // Deutlicher PV-Überschuss: Speicher lädt wahrscheinlich
    isStorageCharging = true;
  } else if (energyBalance < -1.0f) {
    // Deutliches PV-Defizit: Speicher entlädt wahrscheinlich
    isStorageCharging = false;
  }
  // Bei geringem Unterschied: Richtung unverändert lassen
  
  Serial.printf("Richtung: Grid=%s (%.1fkW), Storage=%s (%.1fkW), Balance=%.1fkW\n",
                isGridFeedIn ? "Feed" : "Bezug", gridPower,
                isStorageCharging ? "Laden" : "Entladen", storagePower, energyBalance);
}


void updatePVNetDisplay() {
  // Sensor Index 4 für PV/Netz Display aktualisieren
  if (!isValidSensorIndex(4)) return;

  // Verwende Mischung aus aktuellen und letzten gültigen Werten für stabile Berechnung
  float workingPvPower = pvPower;
  float workingGridPower = gridPower;
  float workingLoadPower = loadPower;
  float workingStoragePower = storagePower;

  unsigned long now = millis();
  const unsigned long MAX_AGE_MS = 300000; // 5 Minuten maximales Alter

  // Verwende letzte gültige Werte wenn aktuelle Werte ungültig oder zu alt sind
  if ((pvPower <= 0.0f || pvPower >= 30.0f) &&
      (now - lastValidPower.pvPowerTime) < MAX_AGE_MS &&
      lastValidPower.pvPower > 0.0f) {
    workingPvPower = lastValidPower.pvPower;
    Serial.printf("📅 Verwende letzten PV-Wert: %.1fkW\n", workingPvPower);
  }

  if ((gridPower < 0.0f || gridPower >= 50.0f) &&
      (now - lastValidPower.gridPowerTime) < MAX_AGE_MS &&
      lastValidPower.gridPower >= 0.0f) {
    workingGridPower = lastValidPower.gridPower;
    Serial.printf("📅 Verwende letzten Grid-Wert: %.1fkW\n", workingGridPower);
  }

  if ((loadPower <= 0.0f || loadPower >= 50.0f) &&
      (now - lastValidPower.loadPowerTime) < MAX_AGE_MS &&
      lastValidPower.loadPower > 0.0f) {
    workingLoadPower = lastValidPower.loadPower;
    Serial.printf("📅 Verwende letzten Load-Wert: %.1fkW\n", workingLoadPower);
  }

  if ((storagePower < 0.0f || storagePower >= 20.0f) &&
      (now - lastValidPower.storagePowerTime) < MAX_AGE_MS &&
      lastValidPower.storagePower >= 0.0f) {
    workingStoragePower = lastValidPower.storagePower;
    Serial.printf("📅 Verwende letzten Storage-Wert: %.1fkW\n", workingStoragePower);
  }

  // Robuste Power-Validierung mit Working-Werten
  bool hasValidData = false;

  // Prüfe ob wir mindestens einen plausiblen Wert haben
  if (workingLoadPower > 0.0f && workingLoadPower < 50.0f) {
    hasValidData = true;
  } else if (workingPvPower > 0.0f && workingPvPower < 30.0f) {
    hasValidData = true;
    // Schätze Verbrauch aus PV wenn loadPower nicht verfügbar
    if (workingLoadPower <= 0.0f || workingLoadPower >= 50.0f) {
      workingLoadPower = max(0.1f, workingPvPower * 0.8f);
      Serial.printf("💡 Verbrauch geschätzt aus PV: %.1fkW\n", workingLoadPower);
    }
  }

  if (!hasValidData) {
    // Fallback: Behalte den letzten gültigen Sensor-Wert bei oder setze Default nur einmal
    if (sensors[4].value <= 0.0f && sensors[4].isTimedOut) {
      sensors[4].value = 2.0f; // Default: 2kW Grundverbrauch, nur wenn noch nie gesetzt
      sensors[4].formattedValue[0] = '\0'; // Force reformatting
      sensors[4].hasChanged = true;
      Serial.println("⚠️ Fallback: Standard-Verbrauch 2kW verwendet (einmalig)");
    }
    // Sensor als nicht-timeout markieren um Display beizubehalten
    sensors[4].isTimedOut = false;
    sensors[4].lastUpdate = millis();
    sensors[4].formatValue();
    renderManager.markSensorChanged(4);
    Serial.printf("🔄 Behalte letzten Verbrauchswert bei: %.1fkW\n", sensors[4].value);
    return;
  }

  // Temporär die Working-Werte in die globalen Variablen kopieren für calculatePowerDirections()
  float originalPvPower = pvPower;
  float originalGridPower = gridPower;
  float originalLoadPower = loadPower;
  float originalStoragePower = storagePower;

  pvPower = workingPvPower;
  gridPower = workingGridPower;
  loadPower = workingLoadPower;
  storagePower = workingStoragePower;
  
  // Richtungen berechnen
  calculatePowerDirections();

  // Sensor-Daten aktualisieren - jetzt mit validiertem Working-Verbrauch
  sensors[4].isTimedOut = false;
  sensors[4].lastUpdate = millis();
  sensors[4].value = workingLoadPower;  // Verwende den validierten Working-Wert

  // Originale Werte wiederherstellen
  pvPower = originalPvPower;
  gridPower = originalGridPower;
  loadPower = originalLoadPower;
  storagePower = originalStoragePower;

  // Sensor als geändert markieren
  sensors[4].hasChanged = true;
  sensors[4].requiresRedraw = true;
  sensors[4].formatValue();  // Wert formatieren
  renderManager.markSensorChanged(4);
  
  
  Serial.printf("✅ PV/Grid Update: Verbrauch %.1fkW (stabil mit Mix aus neuen/alten Werten)\n",
                workingLoadPower);
}

void processDayAheadPriceData(const String& message) {
  Serial.printf("📊 Day-Ahead Preisdaten empfangen (%d Zeichen)\n", message.length());

  // Neues Format: [{"t":timestamp,"v":value},{"t":timestamp,"v":value},...]
  extern DayAheadPriceData dayAheadPrices;

  dayAheadPrices.clear();

  // Aktuelles Datum setzen (da nicht im JSON enthalten)
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  strftime(dayAheadPrices.date, sizeof(dayAheadPrices.date), "%d.%m.%Y", timeinfo);

  Serial.printf("   Datum gesetzt auf: %s\n", dayAheadPrices.date);

  // Parse Array-Format: [{"t":timestamp,"v":value},...]
  if (message.startsWith("[") && message.endsWith("]")) {
    int currentPos = 1; // Nach "["
    int hourIndex = 0;

    while (currentPos < message.length() - 1 && hourIndex < 24) {
      // Suche nach {"t":
      int tStart = message.indexOf("\"t\":", currentPos);
      if (tStart == -1) break;
      tStart += 4;

      // Suche nach Wert (Timestamp)
      int tEnd = message.indexOf(",", tStart);
      if (tEnd == -1) break;

      long timestamp = message.substring(tStart, tEnd).toInt();

      // Suche nach "v":
      int vStart = message.indexOf("\"v\":", tEnd);
      if (vStart == -1) break;
      vStart += 4;

      // Suche nach Wert (Price)
      int vEnd = message.indexOf("}", vStart);
      if (vEnd == -1) break;

      float value = message.substring(vStart, vEnd).toFloat();

      // Konvertiere Timestamp zu Stunde (vereinfacht)
      char hourStr[6];
      snprintf(hourStr, sizeof(hourStr), "%02d:00", hourIndex);

      // Speichere in dayAheadPrices
      if (hourIndex < 24) {
        dayAheadPrices.prices[hourIndex].price = value;
        strncpy(dayAheadPrices.prices[hourIndex].hour, hourStr,
                sizeof(dayAheadPrices.prices[hourIndex].hour) - 1);
        dayAheadPrices.prices[hourIndex].hour[sizeof(dayAheadPrices.prices[hourIndex].hour) - 1] = '\0';
        dayAheadPrices.prices[hourIndex].isValid = true;

        Serial.printf("   [%d] %s: %.2f EUR/MWh\n", hourIndex, hourStr, value);
        hourIndex++;
      }

      // Nächstes Element
      currentPos = message.indexOf(",{", vEnd);
      if (currentPos == -1) break;
      currentPos += 1; // Nach ","
    }

    dayAheadPrices.hasData = (hourIndex > 0);
    dayAheadPrices.lastUpdate = millis();

    Serial.printf("✅ Day-Ahead Daten verarbeitet: %d Preise für %s\n",
                  hourIndex, dayAheadPrices.date);

    // Wenn wir in der Preis-Detail-Ansicht sind, Display aktualisieren
    extern DisplayMode currentMode;
    if (currentMode == PRICE_DETAIL_SCREEN) {
      extern RenderManager renderManager;
      renderManager.markFullRedrawRequired();
    }
  } else {
    Serial.println("❌ Fehler beim Parsen der Day-Ahead Daten - kein prices Array gefunden");
  }
}