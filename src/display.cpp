#include "display.h"
#include "ota.h"
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════════════
//                              TOUCH-MARKIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

struct TouchMarker {
  int x, y;
  unsigned long timestamp;
  bool active;

  TouchMarker() : x(0), y(0), timestamp(0), active(false) {}
};

// Mehrere Touch-Markierungen für Multi-Touch
TouchMarker touchMarkers[5];
int nextMarkerIndex = 0;

// ═══════════════════════════════════════════════════════════════════════════════
//                              HAUPT-RENDER-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

void clearOldElements() {
  // Löscht Elemente an der alten Anti-Burnin Position
  int oldOffsetX = antiBurnin.getLastOffsetX();
  
  // Titel-Bereich löschen
  tft.fillRect(10 + oldOffsetX, 10, 200, 20, Colors::BG_MAIN);
  
  // Alle Sensor-Boxen an alter Position löschen
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    const SensorData& sensor = sensors[i];
    tft.fillRect(sensor.layout.x + oldOffsetX, sensor.layout.y, 
                sensor.layout.w, sensor.layout.h, Colors::BG_MAIN);
  }
  
  // System-Info und andere UI-Elemente
  tft.fillRect(240 + oldOffsetX, 200, Layout::SYSTEM_INFO_WIDTH, Layout::SYSTEM_INFO_HEIGHT + 20, Colors::BG_MAIN);
  tft.fillRect(10 + oldOffsetX, 200, 120, Layout::LINE_SPACING * 3, Colors::BG_MAIN);
  
  // Zeit-Display
  tft.fillRect(240 + oldOffsetX, 8, 80, 32, Colors::BG_MAIN);
  
  Serial.println("Alte UI-Elemente gelöscht");
}

void updateDisplay() {
  if (currentMode == PRICE_DETAIL_SCREEN) {
    drawPriceDetailScreen();
    renderManager.clearAllFlags();
    return;
  }

  if (currentMode != HOME_SCREEN) return;
  
  
  // Anti-Burnin Change erfordert kompletten Redraw
  if (renderManager.changes.antiBurnin) {
    clearOldElements();
    renderManager.markFullRedrawRequired();
    Serial.println("Anti-Burnin Redraw ausgelöst");
  }
  
  if (renderManager.changes.fullScreen || renderManager.fullRedrawRequired) {
    drawHomeScreen();
    renderManager.clearAllFlags();
    systemStatus.performance.totalRedraws++;
    Serial.println("Vollständiger Screen-Redraw");
    return;
  }
  
  // Selektive Updates
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    if (renderManager.changes.sensors[i] || sensors[i].needsRedraw()) {
      drawSensorBox(i);
      sensors[i].markRendered();
      systemStatus.performance.totalRedraws++;
    } else {
      systemStatus.performance.skippedRedraws++;
    }
  }
  
  bool hasUpdates = false;
  
  if (renderManager.changes.systemInfo) {
    drawSystemInfo();
    hasUpdates = true;
  }
  
  if (renderManager.changes.networkStatus) {
    drawNetworkStatus();
    hasUpdates = true;
  }
  
  
  if (renderManager.changes.time) {
    drawTimeDisplay();
    hasUpdates = true;
  }
  
  // OTA-Status ist jetzt in drawNetworkStatus() integriert
  
  // Selektiver Redraw-Ausgabe entfernt - LDR-Wert wird jetzt im main loop alle 3 Sekunden ausgegeben
  
  renderManager.clearAllFlags();
}

void drawHomeScreen() {
  Serial.println("Zeichne Home-Screen...");
  
  tft.fillScreen(Colors::BG_MAIN);
  
  int offsetX = antiBurnin.getOffsetX();
  
  // Titel mit Anti-Burnin Offset
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Smart Home Display Rev2", 10 + offsetX, 10, 2);
  
  // TEST: Zeige kombinierte Layouts als Demonstration
  // Aktiviere dies temporär zum Testen der Kombinationen
  bool testCombinedLayout = false; // DEAKTIVIERT - zurück zum normalen Layout
  
  if (testCombinedLayout) {
    // Test kombinierte Layouts
    Serial.println("🧪 Testing Combined Layouts:");
    
    // Kombiniere erste zwei Felder: Ökostrom + Preis
    drawCombinedSensorBox(0, 1, 10, 35, Layout::SENSOR_BOX_WIDTH_WIDE, Layout::SENSOR_BOX_HEIGHT, "Oeko+Preis");
    
    // Kombiniere Temperaturen: Außen + Wasser
    drawCombinedSensorBox(6, 7, 115, 135, Layout::SENSOR_BOX_WIDTH_WIDE, Layout::SENSOR_BOX_HEIGHT, "Temp");
    
    // Zeichne die nicht kombinierten Sensoren normal
    for (int i = 0; i < System::SENSOR_COUNT; i++) {
      if (i != 0 && i != 1 && i != 6 && i != 7) { // Skip die kombinierten
        drawSensorBox(i);
        sensors[i].markRendered();
      }
    }
    
    // Markiere auch die kombinierten als gerendert
    sensors[0].markRendered();
    sensors[1].markRendered();
    sensors[6].markRendered();
    sensors[7].markRendered();
    
  } else {
    // Normale Layout: Alle Komponenten einzeln zeichnen
    for (int i = 0; i < System::SENSOR_COUNT; i++) {
      drawSensorBox(i);
      sensors[i].markRendered();
    }
  }
  
  drawSystemInfo();
  drawNetworkStatus();  // Enthält jetzt auch OTA-Status
  drawTimeDisplay();
  
  Serial.println("Home-Screen vollständig gezeichnet");
}

void drawPriceDetailScreen() {
  Serial.println("Zeichne Preis-Detail-Screen...");

  tft.fillScreen(Colors::BG_MAIN);

  int offsetX = antiBurnin.getOffsetX();

  // Titel
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Strompreis Day-Ahead", 10 + offsetX, 10, 2);

  // Zurück-Button (oben rechts)
  int backButtonX = 270 + offsetX;
  int backButtonY = 10;
  tft.drawRoundRect(backButtonX, backButtonY, 40, 20, 3, Colors::BORDER_MAIN);
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Zurueck", backButtonX + 3, backButtonY + 6, 1);

  // Aktueller Preis (groß anzeigen)
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("Aktueller Preis:", 10 + offsetX, 40, 1);
  tft.setTextColor(Colors::TEXT_MAIN);
  String currentPriceText = String(sensors[1].formattedValue);
  tft.drawString(currentPriceText, 10 + offsetX, 55, 3);

  // Datum anzeigen
  if (dayAheadPrices.hasData && strlen(dayAheadPrices.date) > 0) {
    tft.setTextColor(Colors::TEXT_LABEL);
    char dateText[64];
    snprintf(dateText, sizeof(dateText), "Datum: %s", dayAheadPrices.date);
    tft.drawString(dateText, 10 + offsetX, 85, 1);
  }

  // Preis-Chart oder Liste
  if (dayAheadPrices.hasData) {
    drawPriceChart(offsetX);
  } else {
    // Keine Daten verfügbar
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("Keine Day-Ahead Daten", 10 + offsetX, 110, 2);
    tft.drawString("verfuegbar", 10 + offsetX, 130, 2);

    // MQTT Topic für Debug anzeigen
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("Topic:", 10 + offsetX, 160, 1);
    tft.drawString("EnergyMarketPriceDayAhead", 10 + offsetX, 175, 1);
  }

  // Status-Info unten
  tft.setTextColor(Colors::TEXT_LABEL);
  if (dayAheadPrices.lastUpdate > 0) {
    unsigned long age = (millis() - dayAheadPrices.lastUpdate) / 1000;
    String ageText = "Update vor: ";
    if (age < 60) {
      ageText += String(age) + "s";
    } else if (age < 3600) {
      ageText += String(age / 60) + "m";
    } else {
      ageText += String(age / 3600) + "h";
    }
    tft.drawString(ageText, 10 + offsetX, 220, 1);
  }

  Serial.println("Preis-Detail-Screen vollständig gezeichnet");
}

void drawPriceChart(int offsetX) {
  // Einfaches Balkendiagramm für 24h Preise
  const int chartX = 10 + offsetX;
  const int chartY = 110;
  const int chartWidth = 300;
  const int chartHeight = 80;
  const int barWidth = chartWidth / 24;

  // Chart-Rahmen
  tft.drawRect(chartX, chartY, chartWidth, chartHeight, Colors::BORDER_MAIN);

  // Finde Min/Max Preise für Skalierung
  float minPrice = 999.0f;
  float maxPrice = -999.0f;
  int validPrices = 0;

  for (int i = 0; i < 24; i++) {
    if (dayAheadPrices.prices[i].isValid) {
      minPrice = min(minPrice, dayAheadPrices.prices[i].price);
      maxPrice = max(maxPrice, dayAheadPrices.prices[i].price);
      validPrices++;
    }
  }

  if (validPrices == 0) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("Keine gueltigen", chartX + 10, chartY + 20, 1);
    tft.drawString("Preisdaten", chartX + 10, chartY + 35, 1);
    return;
  }

  // Preisbereich anzeigen
  tft.setTextColor(Colors::TEXT_LABEL);
  char maxPriceText[32], minPriceText[32];
  snprintf(maxPriceText, sizeof(maxPriceText), "Max: %.2fct", maxPrice);
  snprintf(minPriceText, sizeof(minPriceText), "Min: %.2fct", minPrice);
  tft.drawString(maxPriceText, chartX, chartY - 15, 1);
  tft.drawString(minPriceText, chartX + 100, chartY - 15, 1);

  // Zeichne Balken für jede Stunde
  for (int i = 0; i < 24; i++) {
    if (dayAheadPrices.prices[i].isValid) {
      float price = dayAheadPrices.prices[i].price;

      // Skalierung des Balkens
      float priceRange = maxPrice - minPrice;
      if (priceRange < 0.01f) priceRange = 0.01f;  // Vermeide Division durch Null

      int barHeight = (int)((price - minPrice) / priceRange * (chartHeight - 4));
      barHeight = max(1, barHeight);  // Mindesthöhe

      // Farbe basierend auf Preis (relativ zu Durchschnitt)
      uint16_t barColor;
      float avgPrice = (minPrice + maxPrice) / 2.0f;
      if (price < avgPrice * 0.8f) {
        barColor = Colors::STATUS_GREEN;  // Günstig
      } else if (price > avgPrice * 1.2f) {
        barColor = Colors::STATUS_RED;    // Teuer
      } else {
        barColor = Colors::STATUS_YELLOW; // Mittel
      }

      // Zeichne Balken (von unten nach oben)
      int barX = chartX + 2 + i * barWidth;
      int barY = chartY + chartHeight - 2 - barHeight;

      tft.fillRect(barX, barY, barWidth - 1, barHeight, barColor);

      // Stunden-Label (jede 4. Stunde)
      if (i % 4 == 0) {
        tft.setTextColor(Colors::TEXT_LABEL);
        tft.drawString(String(i), barX, chartY + chartHeight + 2, 1);
      }
    }
  }

  Serial.printf("Preis-Chart gezeichnet: %d gültige Preise (%.2f - %.2f ct)\n",
                validPrices, minPrice, maxPrice);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              SENSOR UND UI-KOMPONENTEN
// ═══════════════════════════════════════════════════════════════════════════════

bool isStockDisplayTime() {
  // Prüft ob die aktuelle Zeit zwischen 8:00 und 22:00 liegt
  if (!systemStatus.timeValid) return true; // Fallback: zeige an wenn Zeit nicht verfügbar
  
  // Sichere Parse der aktuellen Uhrzeit (Format: "HH:MM:SS")
  if (strlen(systemStatus.currentTime) < 5) return true; // Fallback bei ungültiger Zeit
  
  int hour = 0, minute = 0;
  int parsed = sscanf(systemStatus.currentTime, "%d:%d", &hour, &minute);
  
  if (parsed >= 1 && hour >= 0 && hour <= 23) {
    // Zwischen 8:00 und 21:59 anzeigen (22:00 = nicht mehr anzeigen)
    return (hour >= 8 && hour < 22);
  }
  
  return true; // Fallback bei Parse-Fehler oder ungültiger Stunde
}

void drawCombinedSensorBox(int index1, int index2, int x, int y, int width, int height, const char* combinedLabel) {
  // Zeichnet zwei Sensoren in einer gemeinsamen Box
  if (index1 < 0 || index1 >= System::SENSOR_COUNT || 
      index2 < 0 || index2 >= System::SENSOR_COUNT) return;
  
  const SensorData& sensor1 = sensors[index1];
  const SensorData& sensor2 = sensors[index2];
  int offsetX = antiBurnin.getOffsetX();
  
  int boxX = x + offsetX;
  int boxY = y;
  
  // Hintergrundfarbe basierend auf Timeout-Status beider Sensoren
  uint16_t bgColor;
  if (sensor1.isTimedOut && sensor2.isTimedOut) {
    bgColor = Colors::BG_BOX_TIMEOUT;
  } else if (sensor1.isTimedOut || sensor2.isTimedOut) {
    bgColor = Colors::BG_ROW2; // Mischfarbe wenn nur einer offline
  } else {
    bgColor = getRowBackgroundColor(index1); // Normale Reihenfarbe
  }
  
  // Box mit abgerundeten Ecken
  tft.fillRoundRect(boxX, boxY, width, height, Layout::SENSOR_BOX_RADIUS, bgColor);
  tft.drawRoundRect(boxX, boxY, width, height, Layout::SENSOR_BOX_RADIUS, Colors::BORDER_MAIN);
  
  // Kombiniertes Label zeichnen
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString(combinedLabel, boxX + Layout::PADDING_SMALL, boxY + Layout::PADDING_SMALL, 1);
  
  // Erste Sensorwert (links)
  int leftX = boxX + Layout::PADDING_SMALL;
  int valueY = boxY + 15;
  
  if (sensor1.isTimedOut) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("---", leftX, valueY, 1);
  } else {
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString(sensor1.formattedValue, leftX, valueY, 1);
  }
  
  // Trennzeichen
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("|", boxX + width/2 - 3, valueY, 1);
  
  // Zweite Sensorwert (rechts)
  int rightX = boxX + width/2 + 3;
  
  if (sensor2.isTimedOut) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("---", rightX, valueY, 1);
  } else {
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString(sensor2.formattedValue, rightX, valueY, 1);
  }
  
  // Kleine Trend-Pfeile für beide Sensoren (optional)
  if (!sensor1.isTimedOut) {
    drawTrendArrow(boxX + width/4, boxY + Layout::PADDING_SMALL + 5, sensor1.trend, index1);
  }
  if (!sensor2.isTimedOut) {
    drawTrendArrow(boxX + 3*width/4, boxY + Layout::PADDING_SMALL + 5, sensor2.trend, index2);
  }
  
  Serial.printf("🔗 Combined Box: %s [%d:%s=%s|%d:%s=%s]\n", 
               combinedLabel, index1, sensor1.label, sensor1.formattedValue,
               index2, sensor2.label, sensor2.formattedValue);
}

void drawSensorBox(int index) {
  if (index < 0 || index >= System::SENSOR_COUNT) return;
  
  // Spezielle Behandlung für Aktien-Box (Index 2): Nur zwischen 8:00-22:00 anzeigen
  if (index == 2 && !isStockDisplayTime()) {
    // Aktien-Box ausblenden: Bereich löschen
    const SensorData& sensor = sensors[index];
    int offsetX = antiBurnin.getOffsetX();
    int boxX = sensor.layout.x + offsetX;
    int boxY = sensor.layout.y;
    
    tft.fillRect(boxX, boxY, sensor.layout.w, sensor.layout.h, Colors::BG_MAIN);
    
    // Alternativ: Zeige "Börsenschluss" an
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("Boerse", boxX + Layout::PADDING_SMALL, boxY + Layout::PADDING_SMALL, 1);
    tft.drawString("geschl.", boxX + Layout::PADDING_SMALL, boxY + 15, 1);
    return;
  }
  
  const SensorData& sensor = sensors[index];
  int offsetX = antiBurnin.getOffsetX();
  
  int boxX = sensor.layout.x + offsetX;
  int boxY = sensor.layout.y;
  
  // Spezielle Behandlung für intern berechnete Sensoren (Index 4 und 8)
  uint16_t bgColor;
  if (index == 4 || index == 8) {
    // Interne Sensoren: Verwende nie Timeout-Farbe, da sie nicht von MQTT abhängen
    bgColor = getRowBackgroundColor(index);
  } else {
    // Externe Sensoren: Normale Timeout-Behandlung
    bgColor = sensor.isTimedOut ? Colors::BG_BOX_TIMEOUT : getRowBackgroundColor(index);
  }
  
  // Box mit abgerundeten Ecken
  tft.fillRoundRect(boxX, boxY, sensor.layout.w, sensor.layout.h, 
                   Layout::SENSOR_BOX_RADIUS, bgColor);
  tft.drawRoundRect(boxX, boxY, sensor.layout.w, sensor.layout.h, 
                   Layout::SENSOR_BOX_RADIUS, Colors::BORDER_MAIN);
  
  // Label zeichnen
  uint16_t labelColor = sensor.isTimedOut ? Colors::TEXT_TIMEOUT : Colors::TEXT_LABEL;
  tft.setTextColor(labelColor);
  tft.drawString(sensor.label, boxX + Layout::PADDING_SMALL, boxY + Layout::PADDING_SMALL, 1);
  
  // Wert anzeigen (ohne Trend-Farben)
  if (sensor.isTimedOut) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("---", boxX + Layout::PADDING_SMALL, boxY + 15, 2);
  } else {
    if (index == 2) {
      // Aktien-Box: Wert + Prozentänderung anzeigen
      tft.setTextColor(Colors::TEXT_MAIN);
      tft.drawString(sensor.formattedValue, boxX + Layout::PADDING_SMALL, boxY + 12, 2); // 3px höher
      
      // Prozentänderung berechnen und anzeigen
      if (stockPreviousClose > 0.01f) {
        float percentChange = ((sensor.value - stockPreviousClose) / stockPreviousClose) * 100.0f;
        
        // Farbe basierend auf Änderung
        uint16_t percentColor = Colors::TEXT_LABEL; // Standard grau
        if (percentChange > 0.05f) {
          percentColor = Colors::STATUS_GREEN;
        } else if (percentChange < -0.05f) {
          percentColor = Colors::STATUS_RED;
        }
        
        tft.setTextColor(percentColor);
        
        // Formatierung für Platz sparen: ohne "%" falls zu eng
        char percentText[16];
        if (fabs(percentChange) < 10.0f) { // fabs() für float-Werte verwenden
          snprintf(percentText, sizeof(percentText), "%s%.1f%%", percentChange > 0 ? "+" : "", percentChange);
        } else {
          snprintf(percentText, sizeof(percentText), "%s%.0f%%", percentChange > 0 ? "+" : "", percentChange);
        }
        
        // Kleine Schrift für Prozentänderung unter dem Preis
        tft.drawString(percentText, boxX + Layout::PADDING_SMALL, boxY + 28, 1);
      }
    } else if (index == 4) {
      // Verbrauch-Box: Zeige Gesamtverbrauch höher positioniert (wie bei Aktien)
      tft.setTextColor(Colors::TEXT_MAIN);
      char totalText[16];
      snprintf(totalText, sizeof(totalText), "%.1fkW", loadPower);
      tft.drawString(totalText, boxX + Layout::PADDING_SMALL, boxY + 12, 2); // 3px höher wie bei Aktien
    } else {
      tft.setTextColor(Colors::TEXT_MAIN);  // Immer weiß
      tft.drawString(sensor.formattedValue, boxX + Layout::PADDING_SMALL, boxY + 15, 2);
    }
    
    // Trend-Pfeil zeichnen (nur für spezifische Sensoren)
    if (index != 6) { // Kein Pfeil für: Außentemperatur
      int arrowX = boxX + sensor.layout.w - 20;  // 20px vom rechten Rand
      int arrowY = boxY + Layout::PADDING_SMALL + 5;  // Oben in der Box
      drawTrendArrow(arrowX, arrowY, sensor.trend, index);
    }
   }
  
  // Progress Bar für verschiedene Sensoren
  if (sensor.layout.hasProgressBar && !sensor.isTimedOut) {
    float progressValue = sensor.value;
    
    // Skalierung je nach Sensor-Index
    if (index == 4) { // PV-Leistung: 0-10kW = 0-100% (Werte kommen als kW über MQTT)
      progressValue = (sensor.value / 10.0f) * 100.0f;
      progressValue = constrain(progressValue, 0.0f, 100.0f);
      
      // Minimum-Sichtbarkeit: Wenn Wert > 0kW aber < 1%, zeige mindestens 1%
      if (sensor.value > 0 && progressValue < 1.0f) {
        progressValue = 1.0f;
      }
      
      Serial.printf("PV ProgressBar: %.1fkW -> %.1f%% (Skala: 0-10kW)\n", 
                   sensor.value, progressValue);
    }
    // Index 3 (Ladestand) bleibt bei sensor.value (bereits in %)
    
    drawProgressBar(boxX + Layout::PADDING_SMALL, 
                   boxY + sensor.layout.h - 8, 
                   sensor.layout.w - 2 * Layout::PADDING_SMALL, 
                   progressValue, false);
  } else if (index == 4) {
    // Debug für PV-Sensor ohne Progress Bar
    Serial.printf("WARNUNG - PV ProgressBar NICHT gezeichnet: hasProgressBar=%s, isTimedOut=%s\n", 
                 sensor.layout.hasProgressBar ? "JA" : "NEIN", 
                 sensor.isTimedOut ? "JA" : "NEIN");
  }
  
  // Consumption Bar für PV/Netz
  if (sensor.layout.hasBidirectionalBar && !sensor.isTimedOut) {
    // Consumption Bar mit Energiequellen-Segmenten (gleiche Größe wie Progress Bar)
    drawConsumptionBar(boxX + Layout::PADDING_SMALL, 
                      boxY + sensor.layout.h - 8, // Gleiche Position wie normale Progress Bars
                      sensor.layout.w - 2 * Layout::PADDING_SMALL,
                      15.0f);  // Max 15kW
  }
  
  // Indikatoren (Ampeln)
  if (sensor.layout.hasIndicator && !sensor.isTimedOut) {
    int indicatorX = boxX + sensor.layout.w - 15;
    int indicatorY = boxY + 20;
    
    if (index == 0) { // Ökostrom-Ampel
      uint16_t color;
      if (sensor.value >= 75.0f) color = Colors::STATUS_GREEN;
      else if (sensor.value >= 50.0f) color = Colors::STATUS_YELLOW;
      else if (sensor.value >= 25.0f) color = Colors::STATUS_ORANGE;
      else color = Colors::STATUS_RED;
      drawIndicator(indicatorX, indicatorY, color, true);
      
    } else if (index == 5) { // Wassertemperatur-Ampel
      uint16_t color;
      if (sensor.value >= 50.0f) color = Colors::STATUS_GREEN;
      else if (sensor.value >= 48.0f) color = Colors::STATUS_YELLOW;
      else if (sensor.value >= 45.0f) color = Colors::STATUS_ORANGE;
      else color = Colors::STATUS_RED;
      drawIndicator(indicatorX, indicatorY, color, true);
    }
  }
}

uint16_t getTrendColor(SensorData::TrendDirection trend, int index) {
  switch (index) {
    case 1: // Strompreis: niedrigerer Preis = grün
      return (trend == SensorData::DOWN) ? Colors::TREND_UP : 
             (trend == SensorData::UP) ? Colors::TREND_DOWN : Colors::TEXT_MAIN;
    
    case 6: // Außentemperatur: neutral
      return Colors::TEXT_MAIN;
    
    case 2: // Aktienkurs: Vergleich mit stockPreviousClose
      if (stockPreviousClose <= 0.0f) return Colors::TEXT_MAIN;
      return (sensors[index].value > stockPreviousClose) ? Colors::TREND_UP : 
             (sensors[index].value < stockPreviousClose) ? Colors::TREND_DOWN : Colors::TEXT_MAIN;
    
    default: // Standard: höhere Werte = grün
      return (trend == SensorData::UP) ? Colors::TREND_UP : 
             (trend == SensorData::DOWN) ? Colors::TREND_DOWN : Colors::TEXT_MAIN;
  }
}

void drawSystemInfo() {
  int infoX = 240 + antiBurnin.getOffsetX();
  int infoY = 200;
  
  // Erweiterten Bereich löschen
  tft.fillRect(infoX, infoY, Layout::SYSTEM_INFO_WIDTH, Layout::SYSTEM_INFO_HEIGHT + 20, Colors::BG_MAIN);
  
  // RAM-Status - einheitlich grau wie Uptime/LDR
  String memText = "RAM:";
  if (systemStatus.freeHeap >= 1024 * 1024) {
    memText += String(systemStatus.freeHeap / (1024 * 1024)) + "MB";
  } else if (systemStatus.freeHeap >= 1024) {
    memText += String(systemStatus.freeHeap / 1024) + "KB";
  } else {
    memText += String(systemStatus.freeHeap) + "B";
  }
  
  if (systemStatus.lowMemoryWarning) {
    memText += "!";
  }
  
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString(memText, infoX, infoY, 1);
  
  // CPU-Last - einheitlich grau wie Uptime/LDR
  tft.setTextColor(Colors::TEXT_LABEL);
  char cpuText[16];
  snprintf(cpuText, sizeof(cpuText), "CPU:%.0f%%", systemStatus.cpuUsageSmoothed);
  tft.drawString(cpuText, infoX, infoY + Layout::LINE_SPACING, 1);
  
  // Uptime und LDR nebeneinander (platzsparend)
  char uptimeText[16];
  if (systemStatus.uptime < 3600) {
    snprintf(uptimeText, sizeof(uptimeText), "UP:%lum", systemStatus.uptime / 60);
  } else if (systemStatus.uptime < 86400) {
    snprintf(uptimeText, sizeof(uptimeText), "UP:%luh", systemStatus.uptime / 3600);
  } else {
    snprintf(uptimeText, sizeof(uptimeText), "UP:%lud", systemStatus.uptime / 86400);
  }
  
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString(uptimeText, infoX, infoY + 2 * Layout::LINE_SPACING, 1);
  
  // LDR-Wert rechts neben Uptime
  tft.setTextColor(Colors::TEXT_LABEL);
  char ldrText[16];
  snprintf(ldrText, sizeof(ldrText), " LDR:%d", systemStatus.ldrValue);
  tft.drawString(ldrText, infoX + 35, infoY + 2 * Layout::LINE_SPACING, 1);
}

void drawNetworkStatus() {
  int netX = 10 + antiBurnin.getOffsetX();
  int netY = 200;  // Gleiche Höhe wie System-Info
  
  // Erweiterten Bereich löschen (für 3 Zeilen untereinander)
  tft.fillRect(netX, netY, 120, Layout::LINE_SPACING * 3, Colors::BG_MAIN);
  
  // WiFi-Status (erste Zeile)
  if (systemStatus.wifiConnected) {
    tft.setTextColor(Colors::TEXT_LABEL);
    
    char wifiText[32];
    int bars = getSignalBars(systemStatus.wifiRSSI);
    char barText[8] = "";
    for (int i = 0; i < 4; i++) {
      strcat(barText, (i < bars) ? "●" : "○");
    }
    snprintf(wifiText, sizeof(wifiText), "WiFi:%ddBm %s", systemStatus.wifiRSSI, barText);
    
    tft.drawString(wifiText, netX, netY, 1);
  } else {
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("WiFi:FEHLER", netX, netY, 1);
  }
  
  // MQTT-Status (zweite Zeile)
  String mqttText = systemStatus.mqttConnected ? "MQTT:OK" : "MQTT:FEHLER";
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString(mqttText, netX, netY + Layout::LINE_SPACING, 1);
  
  // OTA-Status (dritte Zeile)
  if (isOTAActive()) {
    String otaStatusText = "OTA:" + getOTAStatus();
    int progress = getOTAProgress();
    
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString(otaStatusText, netX, netY + 2 * Layout::LINE_SPACING, 1);
  } else {
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("OTA:Bereit", netX, netY + 2 * Layout::LINE_SPACING, 1);
  }
}


void drawTimeDisplay() {
  int timeX = 240 + antiBurnin.getOffsetX();
  int timeY = 8;
  
  // Alten Bereich löschen
  tft.fillRect(timeX, timeY, 80, 32, Colors::BG_MAIN);
  
  if (systemStatus.timeValid) {
    // Uhrzeit groß anzeigen
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString(systemStatus.currentTime, timeX, timeY, 2);
    
    // Datum kleiner darunter
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString(systemStatus.currentDate, timeX, timeY + 18, 1);
  } else {
    tft.setTextColor(Colors::SYSTEM_ERROR);
    tft.drawString("--:--:--", timeX, timeY, 2);
    tft.drawString("Zeit-Sync", timeX, timeY + 18, 1);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              ECO-VISUALISIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

void drawEcoVisualization(int x, int y) {
  if (loadPower < PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    // Kein Verbrauch - zeige Standby-Symbol
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("~", x, y, 2);
    return;
  }
  
  // Berechne Eco-Score basierend auf Energiequellen
  float totalConsumption = loadPower;
  float pvDirectUse = min(pvPower, totalConsumption);
  float batteryUse = (!isStorageCharging && storagePower > 0) ? 
                     min(storagePower, totalConsumption - pvDirectUse) : 0.0f;
  float gridUse = totalConsumption - pvDirectUse - batteryUse;
  
  // Nachhaltigkeit berechnen (0-100%) - SICHER gegen Division by Zero
  float pvPercent = 0.0f, batteryPercent = 0.0f, gridPercent = 0.0f;
  
  if (totalConsumption > 0.001f) {  // Sichere Prüfung gegen ~0
    pvPercent = (pvDirectUse / totalConsumption) * 100.0f;
    batteryPercent = (batteryUse / totalConsumption) * 100.0f;
    gridPercent = (gridUse / totalConsumption) * 100.0f;
  }
  
  // Eco-Score: PV=100%, Batterie=75% (gespeicherte erneuerbare), Netz=0%
  float ecoScore = (pvPercent * 1.0f + batteryPercent * 0.75f) / 100.0f;
  
  if (ecoScore >= 0.75f) {
    // Sehr nachhaltig: Große grüne Blume (Symbol)
    tft.setTextColor(Colors::STATUS_GREEN);
    tft.fillCircle(x + 8, y + 6, 3, Colors::STATUS_YELLOW);  // Blütenmitte
    tft.drawCircle(x + 8, y + 6, 5, Colors::STATUS_GREEN);   // Blütenrand
    tft.drawFastVLine(x + 8, y + 11, 6, Colors::STATUS_GREEN); // Stiel
    
  } else if (ecoScore >= 0.50f) {
    // Mäßig nachhaltig: Kleine Blume
    tft.setTextColor(Colors::STATUS_YELLOW);
    tft.fillCircle(x + 8, y + 8, 2, Colors::STATUS_YELLOW);  // Kleine Blüte
    tft.drawFastVLine(x + 8, y + 10, 4, Colors::STATUS_GREEN); // Stiel
    
  } else if (ecoScore >= 0.25f) {
    // Wenig nachhaltig: Warndreieck
    tft.setTextColor(Colors::STATUS_ORANGE);
    // Einfaches Dreieck
    tft.drawFastHLine(x + 4, y + 12, 8, Colors::STATUS_ORANGE);
    tft.drawLine(x + 4, y + 12, x + 8, y + 6, Colors::STATUS_ORANGE);
    tft.drawLine(x + 12, y + 12, x + 8, y + 6, Colors::STATUS_ORANGE);
    tft.drawString("!", x + 7, y + 8, 1);
    
  } else {
    // Nicht nachhaltig: Schornstein mit Rauch
    tft.setTextColor(Colors::STATUS_RED);
    // Schornstein (Rechteck)
    tft.drawRect(x + 6, y + 8, 4, 8, Colors::STATUS_RED);
    tft.fillRect(x + 7, y + 9, 2, 6, Colors::STATUS_RED);
    
    // Rauch (kleine Punkte)
    tft.fillCircle(x + 8, y + 6, 1, Colors::TEXT_LABEL);
    tft.fillCircle(x + 10, y + 4, 1, Colors::TEXT_LABEL);
    tft.fillCircle(x + 6, y + 3, 1, Colors::TEXT_LABEL);
  }
  
  // Kein Prozentsatz mehr hier - wird in separater Eco-Box angezeigt
  
  // Debug-Ausgabe
  Serial.printf("Eco-Score: %.0f%% (PV:%.0f%% Bat:%.0f%% Grid:%.0f%%)\n",
                ecoScore * 100, pvPercent, batteryPercent, gridPercent);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              BASIS-UI-ELEMENTE
// ═══════════════════════════════════════════════════════════════════════════════

void drawProgressBar(int x, int y, int width, float percentage, bool showText) {
  tft.drawRect(x, y, width, Layout::PROGRESS_BAR_HEIGHT, Colors::BORDER_PROGRESS);
  tft.fillRect(x + 1, y + 1, width - 2, Layout::PROGRESS_BAR_HEIGHT - 2, Colors::BG_MAIN);
  
  int progressWidth = constrain((width - 2) * (percentage / 100.0f), 0, width - 2);
  uint16_t progressColor;
  
  // Synchronisiert mit Ökostrom-Ampel (schlecht=rot, gut=grün)
  if (percentage >= 75) progressColor = Colors::STATUS_GREEN;      // >= 75%: Grün (gut)
  else if (percentage >= 50) progressColor = Colors::STATUS_YELLOW; // 50-74%: Gelb
  else if (percentage >= 25) progressColor = Colors::STATUS_ORANGE; // 25-49%: Orange
  else progressColor = Colors::STATUS_RED;                         // < 25%: Rot (schlecht)
  
  if (progressWidth > 0) {
    tft.fillRect(x + 1, y + 1, progressWidth, Layout::PROGRESS_BAR_HEIGHT - 2, progressColor);
  }
  
  if (showText) {
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString(String(percentage, 0) + "%", x + width + Layout::PADDING_MEDIUM, y - 2, 1);
  }
}

void drawIndicator(int x, int y, uint16_t color, bool withBorder) {
  tft.fillCircle(x, y, Layout::INDICATOR_RADIUS, color);
  if (withBorder) {
    tft.drawCircle(x, y, Layout::INDICATOR_RADIUS, Colors::BORDER_INDICATOR);
  }
}

void drawTrendArrow(int x, int y, SensorData::TrendDirection trend, int sensorIndex) {
  // Verwende die sensor-spezifische Farb-Logik
  uint16_t arrowColor = getTrendArrowColor(sensorIndex, trend);
  
  // Schickere Pfeil-Geometrie mit verbesserter Größe
  const int arrowSize = 7;  // Etwas größer für bessere Sichtbarkeit
  
  if (trend == SensorData::UP) {
    // Subtiler Glow-Effekt (optional): Größeres, blasseres Dreieck im Hintergrund
    uint16_t glowColor = (arrowColor & 0xF79E) >> 1; // Farbe halbiert für Glow
    tft.fillTriangle(x, y - 3,                    
                     x - (arrowSize+1)/2, y + 5,      
                     x + (arrowSize+1)/2, y + 5,      
                     glowColor);
    
    // Hauptpfeil: Aufwärts-Dreieck (gefüllt)
    tft.fillTriangle(x, y - 2,                    // Spitze oben
                     x - arrowSize/2, y + 4,      // Linke Ecke unten
                     x + arrowSize/2, y + 4,      // Rechte Ecke unten
                     arrowColor);
    
    // Dezenter Rahmen nur bei wichtigen Trends
    if (arrowColor == Colors::TREND_UP_STRONG || arrowColor == Colors::TREND_DOWN_STRONG) {
      tft.drawTriangle(x, y - 2,
                       x - arrowSize/2, y + 4,
                       x + arrowSize/2, y + 4,
                       Colors::TEXT_LABEL);
    }
                     
  } else if (trend == SensorData::DOWN) {
    // Subtiler Glow-Effekt
    uint16_t glowColor = (arrowColor & 0xF79E) >> 1;
    tft.fillTriangle(x, y + 5,                    
                     x - (arrowSize+1)/2, y - 3,      
                     x + (arrowSize+1)/2, y - 3,      
                     glowColor);
    
    // Hauptpfeil: Abwärts-Dreieck (gefüllt)
    tft.fillTriangle(x, y + 4,                    // Spitze unten
                     x - arrowSize/2, y - 2,      // Linke Ecke oben
                     x + arrowSize/2, y - 2,      // Rechte Ecke oben
                     arrowColor);
    
    // Dezenter Rahmen nur bei wichtigen Trends
    if (arrowColor == Colors::TREND_UP_STRONG || arrowColor == Colors::TREND_DOWN_STRONG) {
      tft.drawTriangle(x, y + 4,
                       x - arrowSize/2, y - 2,
                       x + arrowSize/2, y - 2,
                       Colors::TEXT_LABEL);
    }
                     
  } else {
    // Stabil: Eleganter horizontaler Balken mit abgerundeten Enden
    int barWidth = arrowSize + 2;
    int barHeight = 3; // Etwas dicker für bessere Sichtbarkeit
    
    // Subtiler Glow-Hintergrund
    uint16_t glowColor = (arrowColor & 0xF79E) >> 1;
    tft.fillRect(x - (barWidth+2)/2, y - 1, barWidth + 2, barHeight + 2, glowColor);
    
    // Hauptbalken
    tft.fillRect(x - barWidth/2, y, barWidth, barHeight, arrowColor);
    
    // Perfekt abgerundete Enden
    tft.fillCircle(x - barWidth/2, y + barHeight/2, barHeight/2, arrowColor);
    tft.fillCircle(x + barWidth/2, y + barHeight/2, barHeight/2, arrowColor);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              HILFSFUNKTIONEN FÜR FARBEN UND LAYOUT
// ═══════════════════════════════════════════════════════════════════════════════

uint16_t getTrendArrowColor(int index, SensorData::TrendDirection trend) {
  if (index < 0 || index >= System::SENSOR_COUNT) return Colors::TEXT_MAIN;
  
  // Berechne Änderungsstärke für schönere Farbabstufungen
  bool isStrongChange = false;
  if (index < System::SENSOR_COUNT) {
    float changePercent = 0.0f;
    if (sensors[index].lastValue > 0) {
      changePercent = abs(sensors[index].value - sensors[index].lastValue) / sensors[index].lastValue * 100.0f;
      isStrongChange = (changePercent > 5.0f); // > 5% Änderung = "stark"
    }
  }
  
  // Sensor-spezifische Farb-Logik mit Intensitäts-Abstufungen
  switch (index) {
    case 1: // Strompreis: niedriger = grün, höher = rot
      if (trend == SensorData::DOWN) {
        return isStrongChange ? Colors::TREND_UP_STRONG : Colors::TREND_UP;
      } else if (trend == SensorData::UP) {
        return isStrongChange ? Colors::TREND_DOWN_STRONG : Colors::TREND_DOWN;
      } else {
        return Colors::TREND_NEUTRAL;
      }
    
    case 4: // Verbrauch: niedriger = grün, höher = rot  
      if (trend == SensorData::DOWN) {
        return isStrongChange ? Colors::TREND_UP_STRONG : Colors::TREND_UP;
      } else if (trend == SensorData::UP) {
        return isStrongChange ? Colors::TREND_DOWN_STRONG : Colors::TREND_DOWN;
      } else {
        return Colors::TREND_NEUTRAL;
      }
    
    case 0: // Ökostrom: höher = grün, niedriger = rot
    case 3: // Ladestand: höher = grün, niedriger = rot
    case 5: // Wallbox: höher = grün, niedriger = rot
    case 7: // Wasser: höher = grün, niedriger = rot
      if (trend == SensorData::UP) {
        return isStrongChange ? Colors::TREND_UP_STRONG : Colors::TREND_UP;
      } else if (trend == SensorData::DOWN) {
        return isStrongChange ? Colors::TREND_DOWN_STRONG : Colors::TREND_DOWN;
      } else {
        return Colors::TREND_NEUTRAL;
      }
    
    case 2: // Aktien: Vergleich mit stockPreviousClose für besondere Behandlung
      if (trend == SensorData::UP) {
        return isStrongChange ? Colors::TREND_UP_STRONG : Colors::TREND_UP;
      } else if (trend == SensorData::DOWN) {
        return isStrongChange ? Colors::TREND_DOWN_STRONG : Colors::TREND_DOWN;
      } else {
        return Colors::TREND_NEUTRAL;
      }
    
    case 6: // Außentemperatur: KEINE Trendpfeile
      return Colors::TEXT_MAIN; // Wird sowieso nicht gezeichnet
    
    default: // Fallback
      return Colors::TREND_NEUTRAL;
  }
}

void drawConsumptionBar(int x, int y, int width, float maxConsumption) {
  // Verbesserte Consumption Bar - gleiche Größe wie Ladestand-Balken
  const float MIN_SEGMENT_WIDTH = 4.0f; // Etwas kleinere Mindestbreite
  const float MIN_CONSUMPTION_THRESHOLD = 0.02f; // Reduzierter Schwellwert
  int barHeight = Layout::PROGRESS_BAR_HEIGHT; // Gleiche Höhe wie normale Progress Bars
  
  // Hintergrund löschen
  tft.fillRect(x, y - 2, width, barHeight + 4, Colors::BG_MAIN);
  
  // Robuste Verbrauchsberechnung mit Validierung
  float totalConsumption = max(0.0f, loadPower); // Negative Werte abfangen
  
  if (totalConsumption < PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    // Sehr geringer Verbrauch - zeige leeren Balken
    tft.drawRect(x, y, width, barHeight, Colors::BORDER_PROGRESS);
    return;
  }
  
  // Robuste Energiequellen-Berechnung mit Validierung
  float pvDirectUse = max(0.0f, min(max(0.0f, pvPower), totalConsumption));
  float remainingConsumption = max(0.0f, totalConsumption - pvDirectUse);
  
  float batteryUse = 0.0f;
  float gridUse = 0.0f;
  
  if (remainingConsumption > PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    if (!isStorageCharging && storagePower > PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
      // Batterie entlädt - sichere Berechnung
      batteryUse = max(0.0f, min(storagePower, remainingConsumption));
      gridUse = max(0.0f, remainingConsumption - batteryUse);
    } else {
      // Kein Batterieeinsatz oder Batterie lädt
      gridUse = remainingConsumption;
    }
  }
  
  // Sicherheitscheck: Summe darf nicht größer als Gesamtverbrauch sein
  float totalSources = pvDirectUse + batteryUse + gridUse;
  if (totalSources > totalConsumption + 0.01f) {
    // Proportionale Korrektur
    float correctionFactor = totalConsumption / totalSources;
    pvDirectUse *= correctionFactor;
    batteryUse *= correctionFactor;
    gridUse *= correctionFactor;
    Serial.printf("WARNUNG - Consumption Bar: Korrektur angewendet (Faktor: %.3f)\n", correctionFactor);
  }
  
  // Robuste Balkenlängen-Berechnung
  float consumptionRatio = min(totalConsumption / max(1.0f, maxConsumption), 1.0f);
  int totalBarWidth = max(1, (int)(width * consumptionRatio));
  
  // Sichere Segment-Breiten berechnung
  int pvWidth = 0, batteryWidth = 0, gridWidth = 0;
  
  if (totalBarWidth > 0 && totalConsumption > PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    // Berechne Anteile mit zusätzlicher Sicherheit
    float pvRatio = (totalConsumption > 0) ? (pvDirectUse / totalConsumption) : 0.0f;
    float batteryRatio = (totalConsumption > 0) ? (batteryUse / totalConsumption) : 0.0f;
    float gridRatio = max(0.0f, 1.0f - pvRatio - batteryRatio);
    
    // Berechne Pixel-Breiten mit Mindestbreiten
    pvWidth = (pvDirectUse > PowerManagement::MIN_CONSUMPTION_THRESHOLD) ?
              max((int)(pvRatio * totalBarWidth), PowerManagement::POWER_BAR_MIN_WIDTH) : 0;
    batteryWidth = (batteryUse > PowerManagement::MIN_CONSUMPTION_THRESHOLD) ?
                   max((int)(batteryRatio * totalBarWidth), PowerManagement::POWER_BAR_MIN_WIDTH) : 0;
    
    // Grid-Breite als Rest berechnen
    gridWidth = max(0, totalBarWidth - pvWidth - batteryWidth);
    
    // Falls immer noch Überlauf: proportional reduzieren
    if (pvWidth + batteryWidth + gridWidth > totalBarWidth) {
      int overflow = (pvWidth + batteryWidth + gridWidth) - totalBarWidth;
      if (gridWidth >= overflow) {
        gridWidth -= overflow;
      } else {
        // Proportionale Reduktion aller Segmente
        float scale = (float)totalBarWidth / (pvWidth + batteryWidth + gridWidth);
        pvWidth = (int)(pvWidth * scale);
        batteryWidth = (int)(batteryWidth * scale);
        gridWidth = totalBarWidth - pvWidth - batteryWidth;
      }
    }
  }
  
  // Balken zeichnen mit sicherer Positionierung
  int currentX = x;
  
  // PV-Segment (grün)
  if (pvWidth > 0) {
    tft.fillRect(currentX, y, min(pvWidth, width), barHeight, Colors::STATUS_GREEN);
    currentX += pvWidth;
  }
  
  // Batterie-Segment (cyan für bessere Unterscheidung)
  if (batteryWidth > 0 && currentX < x + width) {
    int segmentWidth = min(batteryWidth, (x + width) - currentX);
    tft.fillRect(currentX, y, segmentWidth, barHeight, Colors::STATUS_CYAN);
    currentX += batteryWidth;
  }
  
  // Netz-Segment (orange statt rot für bessere Sichtbarkeit)
  if (gridWidth > 0 && currentX < x + width) {
    int segmentWidth = min(gridWidth, (x + width) - currentX);
    tft.fillRect(currentX, y, segmentWidth, barHeight, Colors::STATUS_ORANGE);
  }
  
  // Rahmen um den gesamten verfügbaren Bereich
  tft.drawRect(x, y, totalBarWidth, barHeight, Colors::BORDER_PROGRESS);
  
  // Keine zusätzlichen Labels - nur der farbige Balken zur Visualisierung
  
  Serial.printf("Robust Consumption: %.1fkW (PV:%.2f Bat:%.2f Grid:%.2f) Widths:(%d,%d,%d)\n",
                totalConsumption, pvDirectUse, batteryUse, gridUse, pvWidth, batteryWidth, gridWidth);
}

void drawBidirectionalBar(int x, int y, int width, float pvPower, float gridPower, float maxPower) {
  // Bidirektionale Balken-Logik mit berechneten Richtungen
  int centerX = x + width / 2;
  int barHeight = PowerManagement::BIDIRECTIONAL_BAR_HEIGHT;
  
  // Hintergrund löschen
  tft.fillRect(x, y, width, barHeight, Colors::BG_MAIN);
  
  // Mittellinie (immer sichtbar)
  tft.drawFastVLine(centerX, y - 1, barHeight + 2, Colors::BORDER_PROGRESS);
  
  // Prüfe ob gridPower nahe Null ist (perfekte Balance)
  bool gridNearZero = (gridPower < PowerManagement::GRID_BALANCE_THRESHOLD);
  
  if (gridNearZero) {
    // Perfekte Energiebilanz - zeige Batterie-Symbol
    tft.setTextColor(Colors::STATUS_CYAN);
    tft.drawString("BAL", centerX - 8, y - 8, 1);
    
    // Dünner cyan Balken in der Mitte
    tft.fillRect(centerX - 5, y + 1, 10, barHeight - 2, Colors::STATUS_CYAN);
    
  } else {
    // Grid-Leistung anzeigen basierend auf berechneter Richtung
    float gridRatio = constrain(gridPower / maxPower, 0.0f, 1.0f);
    int gridBarWidth = (int)((width / 2) * gridRatio);
    
    if (!isGridFeedIn) {
      // Netzbezug nach links (rot)
      tft.fillRect(centerX - gridBarWidth, y, gridBarWidth, barHeight, Colors::STATUS_RED);
      tft.setTextColor(Colors::STATUS_RED);
      tft.drawString("Bezug", x + 2, y - 8, 1);
      
    } else {
      // Netzeinspeisung nach rechts (grün)
      tft.fillRect(centerX + 1, y, gridBarWidth, barHeight, Colors::STATUS_GREEN);
      tft.setTextColor(Colors::STATUS_GREEN);
      tft.drawString("Feed", x + width - 18, y - 8, 1);
    }
  }
  
  // Rahmen um gesamten Balken
  tft.drawRect(x, y, width, barHeight, Colors::BORDER_PROGRESS);
  
  const char* status = gridNearZero ? "[BALANCE]" :
                      (isGridFeedIn ? "[EINSPEISUNG]" : "[BEZUG]");
  Serial.printf("🔄 Grid-Balken: %.1fkW %s (PV=%.1f, Load=%.1f, Storage=%.1f)\n",
               gridPower, status, pvPower, loadPower, storagePower);
}

uint16_t getTimeoutBoxColor(bool isTimedOut) {
  return isTimedOut ? Colors::BG_BOX_TIMEOUT : Colors::BG_BOX;
}

uint16_t getRowBackgroundColor(int sensorIndex) {
  // Reihen-spezifische Hintergrundfarben
  if (sensorIndex >= 0 && sensorIndex <= 2) {
    // Reihe 1: Ökostrom, Preis, Aktie
    return Colors::BG_ROW1;
  } else if (sensorIndex >= 3 && sensorIndex <= 5) {
    // Reihe 2: Ladestand, PV/Netz, Wallbox  
    return Colors::BG_ROW2;
  } else if (sensorIndex >= 6 && sensorIndex <= 7) {
    // Reihe 3: Außen, Wasser
    return Colors::BG_ROW3;
  }
  // Fallback
  return Colors::BG_BOX;
}

uint16_t getRSSIColor(int rssi) {
  if (rssi > -50) return Colors::STATUS_GREEN;
  if (rssi > -60) return Colors::STATUS_YELLOW;
  if (rssi > -70) return Colors::STATUS_ORANGE;
  return Colors::STATUS_RED;
}

uint16_t getSignalBars(int rssi) {
  if (rssi > -50) return 4;
  if (rssi > -60) return 3;
  if (rssi > -70) return 2;
  if (rssi > -80) return 1;
  return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              TOUCH-VISUALISIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

void drawTouchMarker(int x, int y) {
  // Neue Touch-Markierung hinzufügen
  TouchMarker& marker = touchMarkers[nextMarkerIndex];

  // Alte Markierung löschen, falls vorhanden
  if (marker.active) {
    // Lösche alten Kreis (übermale mit Hintergrundfarbe)
    tft.fillCircle(marker.x, marker.y, 12, Colors::BG_MAIN);
  }

  // Neue Markierung setzen
  marker.x = x;
  marker.y = y;
  marker.timestamp = millis();
  marker.active = true;

  // Zeichne Touch-Markierung: Gefüllter Kreis mit Rahmen
  tft.fillCircle(x, y, 10, Colors::TOUCH_MARKER);
  tft.drawCircle(x, y, 10, Colors::TOUCH_BORDER);
  tft.fillCircle(x, y, 5, Colors::TOUCH_BORDER);

  // Koordinaten anzeigen (für Debug)
  char coordText[20];
  snprintf(coordText, sizeof(coordText), "(%d,%d)", x, y);
  tft.setTextColor(Colors::TEXT_MAIN, Colors::BG_MAIN);
  tft.drawString(coordText, x + 15, y - 5, 1);

  Serial.printf("🎯 Touch-Markierung bei (%d,%d) - Marker %d\n", x, y, nextMarkerIndex);

  // Zum nächsten Marker-Slot wechseln (Ring-Buffer)
  nextMarkerIndex = (nextMarkerIndex + 1) % 5;
}

void updateTouchMarkers() {
  unsigned long now = millis();
  bool hasExpiredMarkers = false;

  // Prüfe alle aktiven Marker auf Ablauf
  for (int i = 0; i < 5; i++) {
    TouchMarker& marker = touchMarkers[i];

    if (marker.active && (now - marker.timestamp) > Timing::TOUCH_MARKER_DURATION_MS) {
      // Marker ist abgelaufen - löschen
      tft.fillCircle(marker.x, marker.y, 12, Colors::BG_MAIN);

      // Koordinaten-Text auch löschen
      tft.fillRect(marker.x + 15, marker.y - 8, 50, 16, Colors::BG_MAIN);

      marker.active = false;
      hasExpiredMarkers = true;

      Serial.printf("⏰ Touch-Markierung %d abgelaufen nach %lums\n",
                   i, now - marker.timestamp);
    }
  }

  // Wenn Marker abgelaufen sind, markiere partiellen Redraw
  if (hasExpiredMarkers) {
    renderManager.markFullRedrawRequired();
    Serial.println("🔄 Touch-Marker Redraw ausgelöst");
  }
}


