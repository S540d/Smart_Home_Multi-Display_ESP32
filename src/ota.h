#ifndef OTA_H
#define OTA_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "config_secrets.h"

// ═══════════════════════════════════════════════════════════════════════════════
//                              EXTERNE ABHÄNGIGKEITEN
// ═══════════════════════════════════════════════════════════════════════════════

// Externe Objekte (definiert in main.cpp)
extern SystemStatus systemStatus;
extern RenderManager renderManager;

// ═══════════════════════════════════════════════════════════════════════════════
//                              OTA-KONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

namespace OTAConfig {
  constexpr const char* HOSTNAME = "SmartHomeDisplay";
  constexpr const char* PASSWORD = CONFIG_OTA_PASSWORD;  // Defined in config_secrets.h
  constexpr int PORT = 3232;
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              OTA-STATUS STRUKTUR
// ═══════════════════════════════════════════════════════════════════════════════

struct OTAStatus {
  bool isActive = false;
  bool isInProgress = false;
  String currentOperation = "";
  int progress = 0;
  String lastError = "";
  unsigned long startTime = 0;
  unsigned long lastActivity = 0;
  
  void reset() {
    isActive = false;
    isInProgress = false;
    currentOperation = "";
    progress = 0;
    lastError = "";
    startTime = 0;
    lastActivity = millis();
  }
  
  void updateProgress(int newProgress, const String& operation) {
    progress = newProgress;
    currentOperation = operation;
    lastActivity = millis();
  }
};

extern OTAStatus otaStatus;

// ═══════════════════════════════════════════════════════════════════════════════
//                              OTA-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

// Initialisierung und Setup
void initializeOTA();
void handleOTA();

// Status und Überwachung
bool isOTAActive();
int getOTAProgress();
String getOTAStatus();

// Callback-Funktionen für OTA-Events
void onOTAStart();
void onOTAProgress(unsigned int progress, unsigned int total);
void onOTAEnd();
void onOTAError(ota_error_t error);

// Hilfsfunktionen
void logOTAStatus();
void displayOTAProgress();
String getOTAErrorString(ota_error_t error);

#endif // OTA_H