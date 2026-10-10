#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "display_lgfx.h"
#include "config.h"
#include "state.h"

// ═══════════════════════════════════════════════════════════════════════════════
//                              EXTERNE ABHÄNGIGKEITEN
// ═══════════════════════════════════════════════════════════════════════════════

// Hardware objects (defined in main.cpp)
extern LGFX tft;

// Touch-System für Settings
#include "touch.h"
extern TouchManager touchManager;

// ═══════════════════════════════════════════════════════════════════════════════
//                              DISPLAY-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

// Haupt-Render-Funktionen
void updateDisplay();
void drawGarageAlert();  // Vollbild-Alarm bei offenem Garagentor
void drawAirAlert();     // Vollbild-Alarm "Lüften" bei schlechter Luft (eCO2)
void drawHomeScreen(bool forceFullBg = true);
void drawPriceDetailScreen();
void drawDayAheadDetailScreen();
void drawPriceAnalytics(int offsetX);
void drawSimplePriceChart(int x, int y, int width, int height);
void drawOekostromDetailScreen();
void drawWallboxConsumptionScreen();
void drawLadestandScreen();
void drawSettingsScreen();
#if FEATURE_DEVICES_SCREEN
void drawDevicesScreen();
#endif
void drawPriceChart(int offsetX);
void clearOldElements();

// Sensor und UI-Komponenten
void drawSensorBox(int index);
void drawCombinedSensorBox(int index1, int index2, int x, int y, int width, int height, const char* combinedLabel);
void drawTempCombinedBox(int x, int y, int width, int height);
void drawStorageCombinedBox(int x, int y, int width, int height);
void drawSystemInfo();
void drawNetworkStatus();  // WiFi/MQTT-Status, Zone E neben RAM/Uptime
void drawTimeDisplay();
void drawPlug1Button();  // Zone B rechts: Plug1-Status, nur Einschalten

// Zone E (Status + SmartPlug)
void drawZoneE();

// Zentrale Farbverwaltung
uint16_t getSensorProgressColor(int sensorIndex);

// Basis-UI-Elemente
void drawProgressBar(int x, int y, int width, float percentage, bool showText = false, uint16_t customColor = 0);
void drawConsumptionBar(int x, int y, int width, float maxConsumption);
void drawBidirectionalBar(int x, int y, int width, float pvPower, float gridPower, float maxPower);
void drawPVDistributionBar(int x, int y, int width, float pvPower);
void drawEnergyFlowZone(int x, int y, int w, int h);
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
