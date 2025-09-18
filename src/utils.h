#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════════════════════
//                              EXTERNE ABHÄNGIGKEITEN
// ═══════════════════════════════════════════════════════════════════════════════

// Externe Objekte (definiert in main.cpp)
extern SystemStatus systemStatus;
extern unsigned long systemStartTime;

// ═══════════════════════════════════════════════════════════════════════════════
//                              STRING-FORMATIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

// Zeit-Formatierung
String formatUptime(unsigned long seconds);
String formatTime(unsigned long milliseconds);
String formatDuration(unsigned long startTime, unsigned long endTime);

// Speicher-Formatierung
String formatMemoryValue(uint32_t bytes);
String formatMemoryPercentage(uint32_t used, uint32_t total);

// Werte-Formatierung
String formatFloat(float value, int decimals = 2);
String formatPercentage(float value, int decimals = 1);
String formatFileSize(size_t bytes);

// ═══════════════════════════════════════════════════════════════════════════════
//                              SYSTEM-LOGGING
// ═══════════════════════════════════════════════════════════════════════════════

// System-Informationen
void logSystemInfo();
void logMemoryStatus();
void logPerformanceStats();
void logNetworkInfo();

// Debug-Funktionen
void logFreeHeap(const char* location = "");
void logExecutionTime(unsigned long startTime, const char* operation);

// Erweiterte System-Analyse
void logSystemHealth();
void generateSystemReport();

// ═══════════════════════════════════════════════════════════════════════════════
//                              MATHEMATISCHE HILFSFUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

// Wert-Verarbeitung
float mapFloat(float value, float inMin, float inMax, float outMin, float outMax);
float constrainFloat(float value, float minVal, float maxVal);
int roundToNearest(float value, int base);

// Statistik-Funktionen
float calculateAverage(float values[], int count);
float calculateMovingAverage(float newValue, float oldAverage, float weight = 0.1f);
float calculatePercentageChange(float oldValue, float newValue);

// ═══════════════════════════════════════════════════════════════════════════════
//                              UTILITY-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

// String-Verarbeitung
void safeCopyString(char* dest, const char* src, size_t destSize);
bool isValidString(const char* str, size_t maxLength);
String trimString(const String& str);

// Watchdog und Reset
void feedWatchdog();
void scheduleRestart(unsigned long delayMs = 5000);
bool isSystemStable();

// Hardware-Hilfsfunktionen
int readADCFiltered(int pin, int samples = 5);
float getTemperatureFromADC(int adcValue);
bool isValidWiFiSignal(int rssi);

#endif // UTILS_H