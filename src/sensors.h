#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════════════════════
//                              EXTERNE ABHÄNGIGKEITEN
// ═══════════════════════════════════════════════════════════════════════════════

// Externe Objekte (definiert in main.cpp)
extern SensorData sensors[];
extern RenderManager renderManager;
extern unsigned long systemStartTime;

// ═══════════════════════════════════════════════════════════════════════════════
//                              SENSOR-MANAGEMENT FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

// Sensor-Timeout-Management
void checkSensorTimeouts();

// Sensor-Statistiken und Analyse
void logSensorStatus();
int getOnlineSensorCount();
int getTimeoutSensorCount();

// Sensor-Layout Initialisierung
void initializeSensorLayouts();

// Sensor-Datenvalidierung
bool isValidSensorValue(int index, float value);
void validateSensorRange(int index, float& value);

// Performance-Analyse
struct SensorPerformance {
  unsigned long totalUpdates = 0;
  unsigned long timeoutEvents = 0;
  float averageUpdateInterval = 0.0f;
  unsigned long lastPerformanceCheck = 0;
};

extern SensorPerformance sensorStats[];

void updateSensorPerformance();
void logSensorPerformance();

#endif // SENSORS_H