#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "state.h"

// ═══════════════════════════════════════════════════════════════════════════════
//                              EXTERNE ABHÄNGIGKEITEN
// ═══════════════════════════════════════════════════════════════════════════════

// Hardware objects (defined in main.cpp)
extern WiFiClient espClient;
extern PubSubClient client;

// CPU-Skalierung: Aktivität signalisieren (definiert in main.cpp)
extern void notifyActivity();

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
