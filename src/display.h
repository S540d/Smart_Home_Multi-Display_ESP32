#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════════════════════
//                              EXTERNE ABHÄNGIGKEITEN
// ═══════════════════════════════════════════════════════════════════════════════

// Externe Objekte (definiert in main.cpp)
extern TFT_eSPI tft;
extern SensorData sensors[];
extern SystemStatus systemStatus;
extern AntiBurninManager antiBurnin;
extern RenderManager renderManager;
extern float stockReference;
extern float stockPreviousClose;

// Power Management Variablen
extern float pvPower;
extern float gridPower;
extern float loadPower;
extern float storagePower;
extern float wallboxPower;

// Richtungsinformationen
extern bool isGridFeedIn;
extern bool isStorageCharging;
extern DisplayMode currentMode;

// Price Detail Data
extern DayAheadPriceData dayAheadPrices;

// Touch-System für Settings
#include "touch.h"
extern TouchManager touchManager;

// ═══════════════════════════════════════════════════════════════════════════════
//                              DISPLAY-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

// Haupt-Render-Funktionen
void updateDisplay();
void drawHomeScreen();
void drawPriceDetailScreen();
void drawDayAheadDetailScreen();
void drawPriceAnalytics(int offsetX);
void drawSimplePriceChart(int x, int y, int width, int height);
void drawOekostromDetailScreen();
void drawWallboxConsumptionScreen();
void drawLadestandScreen();
void drawSettingsScreen();
void drawPriceChart(int offsetX);
void clearOldElements();

// Sensor und UI-Komponenten
void drawSensorBox(int index);
void drawCombinedSensorBox(int index1, int index2, int x, int y, int width, int height, const char* combinedLabel);
void drawSystemInfo();
void drawNetworkStatus();  // Enthält jetzt auch OTA-Status
void drawTimeDisplay();

// Basis-UI-Elemente
void drawProgressBar(int x, int y, int width, float percentage, bool showText = false, uint16_t customColor = 0);
void drawConsumptionBar(int x, int y, int width, float maxConsumption);
void drawBidirectionalBar(int x, int y, int width, float pvPower, float gridPower, float maxPower);
void drawPVDistributionBar(int x, int y, int width, float pvPower);
void drawIndicator(int x, int y, uint16_t color, bool withBorder = true);
void drawEcoVisualization(int x, int y);
void drawTrendArrow(int x, int y, SensorData::TrendDirection trend, int sensorIndex);

// Touch-Visualisierung
void drawTouchMarker(int x, int y);
void updateTouchMarkers();

// ═══════════════════════════════════════════════════════════════════════════════
//                              HILFSFUNKTIONEN FÜR FARBEN UND LAYOUT
// ═══════════════════════════════════════════════════════════════════════════════

// Farb-Bestimmung
uint16_t getTrendArrowColor(int index, SensorData::TrendDirection trend);
uint16_t getTimeoutBoxColor(bool isTimedOut);
uint16_t getRowBackgroundColor(int sensorIndex);
uint16_t getRSSIColor(int rssi);

// Netzwerk-Hilfsfunktionen
uint16_t getSignalBars(int rssi);

#endif // DISPLAY_H