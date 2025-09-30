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
      NetworkConfig::ENERGY_MARKET_PRICE_DAY_AHEAD
    };

    for (int i = 0; i < 9; i++) {
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
    if (strlen(NetworkConfig::TOPIC_DATA[i]) > 0 && strcmp(topic, NetworkConfig::TOPIC_DATA[i]) == 0) {
      updateSensorValue(i, message.toFloat());
      return;
    }
  }
  
  // Spezielle Topics verarbeiten
  if (strcmp(topic, NetworkConfig::STOCK_REFERENCE) == 0) {
    float newRef = message.toFloat();
    if (stockReference != newRef) {
      stockReference = newRef;
      Serial.printf("📈 Aktien-Referenz aktualisiert: %.2f€\n", stockReference);
      // Aktienkurs neu bewerten falls online
      if (!sensors[2].isTimedOut) {
        renderManager.markSensorChanged(2);
      }
    }
  } else if (strcmp(topic, NetworkConfig::STOCK_PREVIOUS_CLOSE) == 0) {
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
  } else if (strcmp(topic, NetworkConfig::PV_POWER) == 0) {
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

  } else if (strcmp(topic, NetworkConfig::GRID_POWER) == 0) {
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

  } else if (strcmp(topic, NetworkConfig::LOAD_POWER) == 0) {
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

  } else if (strcmp(topic, NetworkConfig::STORAGE_POWER) == 0) {
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

  } else if (strcmp(topic, NetworkConfig::WALLBOX_POWER) == 0) {
    float newWallboxPower = abs(message.toFloat()); // Immer positiver Wert
    if (wallboxPower != newWallboxPower) {
      wallboxPower = newWallboxPower;
      // Speichere als letzten gültigen Wert wenn plausibel
      if (wallboxPower >= 0.0f && wallboxPower < 30.0f) { // Max 30kW für Wallbox
        lastValidPower.wallboxPower = wallboxPower;
        lastValidPower.wallboxPowerTime = millis();
      }
      Serial.printf("Wallbox-Leistung: %.1fkW\n", wallboxPower);
      // Markiere PV-Sensor für Update (Index 5) da die PV-Distribution davon abhängt
      renderManager.markSensorChanged(5);
    }

  // Calendar functionality removed
    
  } else if (strcmp(topic, NetworkConfig::HISTORY_RESPONSE) == 0) {
    Serial.printf("History-Response: %s\n", message.c_str());
    // Note: History screen feature to be implemented in future version

  } else if (strcmp(topic, NetworkConfig::ENERGY_MARKET_PRICE_DAY_AHEAD) == 0) {
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
  
  // Spezielle Einheitenkonvertierung für Preise entfernt
  // Day-Ahead-Preise kommen bereits in ct/kWh und müssen nicht konvertiert werden

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
  float workingWallboxPower = wallboxPower;

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

  // Wallbox Power Fallback
  if ((wallboxPower < 0.0f || wallboxPower >= 30.0f) &&
      (now - lastValidPower.wallboxPowerTime) < MAX_AGE_MS &&
      lastValidPower.wallboxPower >= 0.0f) {
    workingWallboxPower = lastValidPower.wallboxPower;
    Serial.printf("📅 Verwende letzten Wallbox-Wert: %.1fkW\n", workingWallboxPower);
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
  float originalWallboxPower = wallboxPower;

  pvPower = workingPvPower;
  gridPower = workingGridPower;
  loadPower = workingLoadPower;
  storagePower = workingStoragePower;
  wallboxPower = workingWallboxPower;
  
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
  wallboxPower = originalWallboxPower;

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

      // Speichere in dayAheadPrices (verwende hour direkt als Index)
      if (hour >= 0 && hour < 24) {
        char hourStr[6];
        snprintf(hourStr, sizeof(hourStr), "%02d:00", hour);

        dayAheadPrices.prices[hour].price = value;
        strncpy(dayAheadPrices.prices[hour].hour, hourStr,
                sizeof(dayAheadPrices.prices[hour].hour) - 1);
        dayAheadPrices.prices[hour].hour[sizeof(dayAheadPrices.prices[hour].hour) - 1] = '\0';
        dayAheadPrices.prices[hour].isValid = true;

        Serial.printf("   Stunde %02d:00: %.2f ct/kWh\n", hour, value);
        entryCount++;
      }

      // Nächstes Element
      currentPos = message.indexOf(",{", vEnd);
      if (currentPos == -1) break;
      currentPos += 1; // Nach ","
    }

    dayAheadPrices.hasData = (entryCount > 0);
    dayAheadPrices.lastUpdate = millis();

    // Calculate analytics for enhanced insights
    if (dayAheadPrices.hasData) {
      dayAheadPrices.calculateAnalytics();
      Serial.printf("📊 Analytics: Avg=%.1f¢, Min=%.1f¢@%02d:00, Max=%.1f¢@%02d:00, Quality=%d%%\n",
                    dayAheadPrices.dailyAverage, dayAheadPrices.minPrice, dayAheadPrices.cheapestHour,
                    dayAheadPrices.maxPrice, dayAheadPrices.expensiveHour, dayAheadPrices.dataQuality);

      // Log optimal windows
      for (int i = 0; i < 3; i++) {
        if (dayAheadPrices.optimalWindows[i].isAvailable) {
          Serial.printf("🎯 Optimal Window %d: %02d:00-%02d:00 (Avg: %.1f¢, Save: %.1f¢)\n",
                        i + 1, dayAheadPrices.optimalWindows[i].startHour,
                        dayAheadPrices.optimalWindows[i].endHour,
                        dayAheadPrices.optimalWindows[i].averagePrice,
                        dayAheadPrices.optimalWindows[i].savingsVsPeak);
        }
      }

      // Log trend info
      const char* trendStr = (dayAheadPrices.trend == TREND_RISING) ? "📈 Rising" :
                            (dayAheadPrices.trend == TREND_FALLING) ? "📉 Falling" :
                            "📊 Stable";
      Serial.printf("%s trend, Volatility: %.1f%%, Potential savings: %.1f¢\n",
                    trendStr, dayAheadPrices.volatilityIndex, dayAheadPrices.potentialSavings);
    }

    Serial.printf("✅ Day-Ahead Daten verarbeitet: %d Preise für %s\n",
                  entryCount, dayAheadPrices.date);

    // Aktuellen Preis für Haupt-Display extrahieren und in Sensor[1] setzen
    if (entryCount > 0) {
      // Finde aktuelle Stunde
      time_t nowTime = time(nullptr);
      struct tm* currentTimeInfo = localtime(&nowTime);
      int currentHour = currentTimeInfo->tm_hour;

      // Verwende aktuellen Preis wenn verfügbar, sonst ersten verfügbaren
      float currentDayAheadPrice = 0.0f;
      bool foundCurrentPrice = false;

      if (currentHour < 24 && dayAheadPrices.prices[currentHour].isValid) {
        currentDayAheadPrice = dayAheadPrices.prices[currentHour].price;
        foundCurrentPrice = true;
        Serial.printf("📈 Aktueller Day-Ahead Preis (%02d:00): %.2f ct/kWh\n", currentHour, currentDayAheadPrice);
      } else {
        // Fallback: Finde ersten gültigen Preis
        for (int i = 0; i < 24; i++) {
          if (dayAheadPrices.prices[i].isValid) {
            currentDayAheadPrice = dayAheadPrices.prices[i].price;
            Serial.printf("📈 Day-Ahead Preis (Fallback %02d:00): %.2f ct/kWh\n", i, currentDayAheadPrice);
            break;
          }
        }
      }

      // Update Sensor[1] (Preis-Box) mit aktuellem Day-Ahead Preis
      if (currentDayAheadPrice > 0.0f) {
        updateSensorValue(1, currentDayAheadPrice);
        Serial.printf("🔄 Preis-Sensor aktualisiert: %.2f ct/kWh\n", currentDayAheadPrice);
      }
    }

    // Rate-Limited Display Update: Nur alle 30 Sekunden, um Touch-Konflikte zu vermeiden
    static unsigned long lastDisplayUpdate = 0;
    unsigned long now = millis();

    extern DisplayMode currentMode;
    if (currentMode == DAYAHEAD_SCREEN &&
        (now - lastDisplayUpdate) > 30000) { // 30 Sekunden Mindestabstand
      extern RenderManager renderManager;
      renderManager.markFullRedrawRequired();
      lastDisplayUpdate = now;
      Serial.println("🔄 Day-Ahead Display Update (rate-limited)");
    }
  } else {
    Serial.println("❌ Fehler beim Parsen der Day-Ahead Daten - kein prices Array gefunden");
  }
}