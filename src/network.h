#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════════════════════
//                              EXTERNE ABHÄNGIGKEITEN
// ═══════════════════════════════════════════════════════════════════════════════

// Externe Objekte (definiert in main.cpp)
extern WiFiClient espClient;
extern PubSubClient client;
extern SystemStatus systemStatus;
extern RenderManager renderManager;
extern SensorData sensors[];
extern float stockReference;
extern float stockPreviousClose;

// Power Management Variablen (alle Werte in kW)
extern float pvPower;          // Aktuelle PV-Erzeugungsleistung (immer positiv)
extern float gridPower;        // Aktuelle Netzleistung (Betrag, Richtung über isGridFeedIn)
extern float loadPower;        // Aktueller Hausverbrauch (immer positiv)
extern float storagePower;     // Aktuelle Speicherleistung (Betrag, Richtung über isStorageCharging)

// Richtungsinformationen für Energiefluss
extern bool isGridFeedIn;      // true = Netzeinspeisung, false = Netzbezug
extern bool isStorageCharging; // true = Speicher lädt, false = Speicher entlädt
extern unsigned long lastWifiReconnect;

// Struktur für letzte gültige Power-Werte (Backup für MQTT-Ausfälle)
struct LastValidPowerValues {
  float pvPower;                 // Letzter gültiger PV-Erzeugungswert (kW)
  float gridPower;               // Letzter gültiger Netzleistungswert (kW)
  float loadPower;               // Letzter gültiger Verbrauchswert (kW)
  float storagePower;            // Letzter gültiger Speicherleistungswert (kW)
  unsigned long pvPowerTime;     // Zeitstempel des letzten PV-Updates (millis())
  unsigned long gridPowerTime;   // Zeitstempel des letzten Netz-Updates (millis())
  unsigned long loadPowerTime;   // Zeitstempel des letzten Verbrauchs-Updates (millis())
  unsigned long storagePowerTime; // Zeitstempel des letzten Speicher-Updates (millis())
};
extern LastValidPowerValues lastValidPower;

// ═══════════════════════════════════════════════════════════════════════════════
//                              NETZWERK-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

// WiFi-Management
void connectWiFi();
void reconnectWiFi();
bool checkWifiConnection();

// MQTT-Management
void reconnectMQTT();
void onMqttMessage(char* topic, byte* payload, unsigned int length);
void processMqttMessage(const char* topic, const String& message);

// Sensor-Datenverarbeitung (aus MQTT)
void updateSensorValue(int index, float newValue);
void processDayAheadPriceData(const String& message);

// Hilfsfunktionen
bool isValidSensorIndex(int index);
void updatePVNetDisplay();

#endif // NETWORK_H