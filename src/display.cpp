#include "display.h"
#include "ota.h"
#include "state.h"
#include <cmath>

static SystemState& appState = SystemState::getInstance();

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
TouchMarker touchMarkers[Layout::MAX_TOUCH_MARKERS];
int nextMarkerIndex = 0;

// ═══════════════════════════════════════════════════════════════════════════════
//                              HAUPT-RENDER-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

void clearOldElements() {
  // Phase 3: Anti-Burnin-Shift löscht zonen-weise den alten Offset-Bereich.
  // Da drawHomeScreen() fillScreen() aufruft, reicht es hier, den alten
  // Offset-Streifen (max 2px) zu löschen – die eigentliche Neukompilierung
  // erfolgt durch den anschließenden fullRedraw.
  const int W = 320;
  const int H = 240;
  int oldOX = appState.antiBurnin.getLastOffsetX();
  int oldOY = appState.antiBurnin.getLastOffsetY();

  auto safeFillRect = [&](int x, int y, int w, int h) {
    x = constrain(x, 0, W - 1);
    y = constrain(y, 0, H - 1);
    w = constrain(w, 0, W - x);
    h = constrain(h, 0, H - y);
    if (w > 0 && h > 0) tft.fillRect(x, y, w, h, Colors::BG_MAIN);
  };

  // Zone A: Header
  safeFillRect(oldOX, oldOY, W, 28);
  // Zone B: Sensor-Boxen (mit altem Offset)
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    const SensorData& sensor = appState.sensors[i];
    if (sensor.layout.w > 0 && sensor.layout.h > 0) {
      safeFillRect(sensor.layout.x + oldOX, sensor.layout.y + oldOY,
                   sensor.layout.w, sensor.layout.h);
    }
  }
  // Zone C: Energie-Flow-Zone
  safeFillRect(2 + oldOX, 74 + oldOY, 316, 62);
  // Zone D: Temperaturen
  safeFillRect(oldOX, 136 + oldOY, W, 63);
  // Zone E
  safeFillRect(oldOX, 200 + oldOY, W, 40);

  Serial.println("Alte UI-Elemente gelöscht (Phase-3)");
}

void drawGarageAlert() {
  const uint16_t W = 320, H = 240;
  tft.fillRect(0, 0, W, H, Colors::STATUS_RED);
  tft.drawRect(0, 0, W, H, Colors::TEXT_MAIN);
  tft.drawRect(1, 1, W - 2, H - 2, Colors::TEXT_MAIN);
  tft.drawRect(2, 2, W - 4, H - 4, Colors::TEXT_MAIN);

  tft.setTextColor(Colors::TEXT_MAIN, Colors::STATUS_RED);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.drawString("GARAGENTOR", W / 2, 60, 4);
  tft.setTextSize(3);
  tft.drawString("OFFEN", W / 2, 120, 4);
  tft.setTextSize(1);
  tft.drawString("Tippen zum Ausblenden", W / 2, 205, 2);
  tft.setTextDatum(textdatum_t::top_left);
}

void drawAirAlert() {
  const uint16_t W = 320, H = 240;
  const uint16_t bg = Colors::STATUS_YELLOW;
  const uint16_t fg = TFT_BLACK;
  tft.fillRect(0, 0, W, H, bg);
  tft.drawRect(0, 0, W, H, fg);
  tft.drawRect(1, 1, W - 2, H - 2, fg);
  tft.drawRect(2, 2, W - 4, H - 4, fg);

  char buf[24];
  snprintf(buf, sizeof(buf), "CO2: %d ppm", appState.air.eco2);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextSize(2);
  tft.drawString("LUEFTEN!", W / 2, 80, 4);
  tft.setTextSize(1);
  tft.drawString(buf, W / 2, 150, 4);
  tft.drawString("Tippen zum Ausblenden", W / 2, 205, 2);
  tft.setTextDatum(textdatum_t::top_left);
}

void updateDisplay() {
  // Garagentor offen: Alarm-Kasten statt aller Screens (einmal zeichnen, dann
  // nichts mehr überschreiben, bis er quittiert oder das Tor geschlossen wird).
  if (appState.garage.alertActive()) {
    if (!appState.garage.alertDrawn) {
      drawGarageAlert();
      appState.garage.alertDrawn = true;
      appState.air.alertDrawn = false;  // Lüften-Kasten ggf. neu zeichnen, wenn quittiert
    }
    appState.renderManager.clearAllFlags();
    return;
  }

  // Schlechte Luft: gleiches Verhalten, Garagentor hat aber Vorrang.
  if (appState.air.alertActive()) {
    if (!appState.air.alertDrawn) {
      drawAirAlert();
      appState.air.alertDrawn = true;
    }
    appState.renderManager.clearAllFlags();
    return;
  }

  switch (appState.currentMode) {
    case PRICE_DETAIL_SCREEN:
      drawPriceDetailScreen();
      appState.renderManager.clearAllFlags();
      return;

    case OEKOSTROM_DETAIL_SCREEN:
      drawOekostromDetailScreen();
      appState.renderManager.clearAllFlags();
      return;

    case WALLBOX_CONSUMPTION_SCREEN:
      drawWallboxConsumptionScreen();
      appState.renderManager.clearAllFlags();
      return;

    case LADESTAND_SCREEN:
      drawLadestandScreen();
      appState.renderManager.clearAllFlags();
      return;

    case DAYAHEAD_SCREEN:
      drawPriceDetailScreen();
      appState.renderManager.clearAllFlags();
      return;

    case SETTINGS_SCREEN:
      drawSettingsScreen();
      appState.renderManager.clearAllFlags();
      return;

#if FEATURE_DEVICES_SCREEN
    case DEVICES_SCREEN:
      drawDevicesScreen();
      appState.renderManager.clearAllFlags();
      return;
#endif

    case HOME_SCREEN:
      break;

    default:
      return;
  }
  
  
  // Anti-Burnin Change: selektives Löschen + Redraw ohne fillScreen (Phase 2.2)
  if (appState.renderManager.changes.antiBurnin) {
    clearOldElements();
    drawHomeScreen(false); // kein fillScreen – nur verschobene Zonen neu zeichnen
    appState.renderManager.clearAllFlags();
    appState.systemStatus.performance.totalRedraws++;
    Serial.println("Anti-Burnin Dirty-Rect Redraw");
    return;
  }

  if (appState.renderManager.changes.fullScreen || appState.renderManager.fullRedrawRequired) {
    drawHomeScreen(true);
    appState.renderManager.clearAllFlags();
    appState.systemStatus.performance.totalRedraws++;
    Serial.println("Vollständiger Screen-Redraw");
    return;
  }
  
  // Selektive Updates
  bool zoneCUpdated = false;
  bool zoneDTempUpdated = false;
  bool zoneDStorageUpdated = false;
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    if (appState.renderManager.changes.sensors[i] || appState.sensors[i].needsRedraw()) {
      // Sensoren 4, 5 sind in Zone C integriert – drawEnergyFlowZone verwenden
      if (i == 4 || i == 5) {
        if (!zoneCUpdated) {
          int ox = appState.antiBurnin.getOffsetX();
          int oy = appState.antiBurnin.getOffsetY();
          drawEnergyFlowZone(2 + ox, 74 + oy, 316, 62);
          zoneCUpdated = true;
        }
        appState.sensors[i].markRendered();
      } else if (i == 6 || i == 7) {
        // Außentemp/Wassertemp – kombinierte Icon-Box statt generischer drawSensorBox()
        if (!zoneDTempUpdated) {
          drawTempCombinedBox(2, 136, 156, 63);
          zoneDTempUpdated = true;
        }
        appState.sensors[i].markRendered();
      } else if (i == 3) {
        // Speicher – kombinierte Icon-Box statt generischer drawSensorBox()
        if (!zoneDStorageUpdated) {
          drawStorageCombinedBox(160, 136, 158, 63);
          zoneDStorageUpdated = true;
        }
        appState.sensors[i].markRendered();
      } else {
        drawSensorBox(i);
        appState.sensors[i].markRendered();
      }
      appState.systemStatus.performance.totalRedraws++;
    } else {
      appState.systemStatus.performance.skippedRedraws++;
    }
  }
  
  bool hasUpdates = false;
  
  if (appState.renderManager.changes.systemInfo) {
    drawSystemInfo();
    hasUpdates = true;
  }
  
  if (appState.renderManager.changes.networkStatus) {
    drawNetworkStatus();
    hasUpdates = true;
  }
  
  
  if (appState.renderManager.changes.time) {
    drawTimeDisplay();
    hasUpdates = true;
  }
  
  // OTA-Status ist jetzt in drawNetworkStatus() integriert
  
  // Selektiver Redraw-Ausgabe entfernt - LDR-Wert wird jetzt im main loop alle 3 Sekunden ausgegeben
  
  appState.renderManager.clearAllFlags();
}

void drawHomeScreen(bool forceFullBg) {
  Serial.println("Zeichne Home-Screen (Phase-3)...");

  if (forceFullBg) {
    tft.fillScreen(Colors::BG_MAIN);
  }

  int ox = appState.antiBurnin.getOffsetX();
  int oy = appState.antiBurnin.getOffsetY();

  // ── Zone A: Header (y=0..27) ─────────────────────────────────────────────
  // Zeit links, Netzstatus rechts – gezeichnet von drawTimeDisplay() + drawNetworkStatus()
  drawTimeDisplay();
  drawNetworkStatus();

  // Trennlinie unter Zone A
  tft.drawFastHLine(max(0, ox), 27 + oy, 320 - abs(ox), Colors::BORDER_INDICATOR);

  // ── Zone B: Preis | Ökostrom | Drucker-Button (y=28..73, je 104px) ──────
  drawSensorBox(1); // Preis links        (2,28,104,46)
  drawSensorBox(0); // Ökostrom Mitte   (108,28,104,46)
  // Plug1-Button (Drucker + Printserver) rechts in Zone B (x=214, w=104, h=46).
  // Zeigt nur den Zustand von Plug1 und kann ihn ausschliesslich EINschalten -
  // Ausschalten ist hier bewusst nicht möglich (Printserver hängt mit dran).
  drawPlug1Button();
  appState.sensors[0].markRendered();
  appState.sensors[1].markRendered();
  appState.sensors[2].markRendered();

  // Trennlinie unter Zone B
  tft.drawFastHLine(max(0, ox), 73 + oy, 320 - abs(ox), Colors::BORDER_INDICATOR);

  // ── Zone C: Energie-Flow-Zone (y=74..135) ─────────────────────────────────
  drawEnergyFlowZone(2 + ox, 74 + oy, 316, 62);
  appState.sensors[4].markRendered();
  appState.sensors[5].markRendered();

  // Trennlinie unter Zone C
  tft.drawFastHLine(max(0, ox), 135 + oy, 320 - abs(ox), Colors::BORDER_INDICATOR);

  // ── Zone D: Temperaturen (links) | Speicher (rechts) (y=136..199) ────────
  // Um 50% höher als ursprünglich (42→63px), damit Icon + DejaVu18-Text pro
  // Reihe genug Platz haben und sich nicht überlappen. Der Platz kommt aus
  // Zone E, die bisher nur eine einzelne Textzeile am unteren Rand nutzte.
  drawTempCombinedBox(2, 136, 156, 63);      // Außentemp oben, Wassertemp unten
  drawStorageCombinedBox(160, 136, 158, 63); // Hausspeicher oben, PKW-Ladestand unten
  appState.sensors[3].markRendered();
  appState.sensors[6].markRendered();
  appState.sensors[7].markRendered();

  // Trennlinie unter Zone D
  tft.drawFastHLine(max(0, ox), 199 + oy, 320 - abs(ox), Colors::BORDER_INDICATOR);

  // ── Zone E: Status + SmartPlug-Buttons (y=200..239) ───────────────────────
  drawZoneE();

  drawSystemInfo();

  Serial.println("Home-Screen (Phase-3) vollständig gezeichnet");
}

void drawPlug1Button() {
  using namespace Plug1ButtonLayout;
  int ox = appState.antiBurnin.getOffsetX();
  int oy = appState.antiBurnin.getOffsetY();
  const int x = BTN_X + ox;
  const int y = BTN_Y + oy;
  const SmartPlugState& plug = appState.smartPlugs[0];

  // Nach einem Einschalt-Tipp bis zur Rückmeldung (NodeRed pollt alle 60 s)
  // "schaltet ein" statt weiter "AUS" zeigen, damit nicht mehrfach getippt wird.
  bool pending = !plug.isOn && plug.onRequestedAt != 0 &&
                 (millis() - plug.onRequestedAt) < SmartPlugConfig::ON_PENDING_MS;

  uint16_t bg = plug.isTimedOut ? Colors::BG_BOX_TIMEOUT
              : plug.isOn        ? 0x0320   // gedecktes Grün
              :                    0x2104;  // dunkelgrau
  tft.fillRoundRect(x, y, BTN_W, BTN_H, Layout::SENSOR_BOX_RADIUS, bg);
  tft.drawRoundRect(x, y, BTN_W, BTN_H, Layout::SENSOR_BOX_RADIUS, Colors::BORDER_MAIN);

  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString(plug.name, x + Layout::PADDING_SMALL, y + Layout::PADDING_SMALL, 1);

  const char* stateText;
  uint16_t stateColor;
  if (pending)              { stateText = "schaltet";  stateColor = Colors::STATUS_YELLOW; }
  else if (plug.isTimedOut) { stateText = "Offline";   stateColor = Colors::TEXT_TIMEOUT; }
  else if (plug.isOn)       { stateText = "EIN";       stateColor = Colors::STATUS_GREEN; }
  else                      { stateText = "AUS";       stateColor = Colors::TEXT_MAIN; }
  tft.setTextColor(stateColor);
  tft.drawString(stateText, x + Layout::PADDING_SMALL, y + 16, 2);

  // Handlungshinweis nur, wenn Tippen etwas bewirkt (aus bzw. unbekannt)
  if (!plug.isOn && !pending) {
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("tippen: an", x + Layout::PADDING_SMALL, y + 35, 1);
  }
}

void drawPriceDetailScreen() {
  Serial.println("Zeichne Preis-Detail-Screen...");

  tft.fillScreen(Colors::BG_MAIN);

  int offsetX = appState.antiBurnin.getOffsetX();
  int offsetY = appState.antiBurnin.getOffsetY();

  // Titel
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Strompreis Day-Ahead", Layout::PADDING_LARGE + offsetX, Layout::PADDING_LARGE + offsetY, 2);

  // Zurück-Button (oben rechts)
  int backButtonX = 270 + offsetX;
  int backButtonY = 10 + offsetY;
  tft.drawRoundRect(backButtonX, backButtonY, 40, 20, 3, Colors::BORDER_MAIN);
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Zurueck", backButtonX + 3, backButtonY + 6, 1);

  // Smart Analytics Display
  // "Aktueller Preis" bewusst weggelassen (steht bereits groß auf dem Home-Screen)
  // - schafft Platz, damit der Chart unten nicht mit variablem Content kollidiert.
  if (appState.dayAheadPrices.hasData && appState.dayAheadPrices.lastAnalysis > 0) {
    drawPriceAnalytics(offsetX);
  } else if (appState.dayAheadPrices.hasData) {
    // Datum anzeigen (Legacy-Modus)
    tft.setTextColor(Colors::TEXT_LABEL);
    char dateText[64];
    snprintf(dateText, sizeof(dateText), "Datum: %s", appState.dayAheadPrices.date);
    tft.drawString(dateText, 10 + offsetX, 40, 1);

    drawPriceChart(offsetX);
  } else {
    // Keine Daten verfügbar
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("Keine Day-Ahead Daten", Layout::PADDING_LARGE + offsetX, 60, 2);
    tft.drawString("verfuegbar", Layout::PADDING_LARGE + offsetX, 85, 2);

    // MQTT Topic für Debug anzeigen
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("Topic:", Layout::PADDING_LARGE + offsetX, 120, 1);
    tft.drawString("EnergyMarketPriceDayAhead", Layout::PADDING_LARGE + offsetX, 135, 1);
  }

  // Status-Info unten
  tft.setTextColor(Colors::TEXT_LABEL);
  if (appState.dayAheadPrices.lastUpdate > 0) {
    unsigned long age = (millis() - appState.dayAheadPrices.lastUpdate) / 1000;
    char ageText[32];
    if (age < 60) {
      snprintf(ageText, sizeof(ageText), "Update vor: %lus", age);
    } else if (age < 3600) {
      snprintf(ageText, sizeof(ageText), "Update vor: %lum", age / 60);
    } else {
      snprintf(ageText, sizeof(ageText), "Update vor: %luh", age / 3600);
    }
    tft.drawString(ageText, Layout::PADDING_LARGE + offsetX, 220, 1);
  }

  Serial.println("Preis-Detail-Screen vollständig gezeichnet");
}

void drawPriceAnalytics(int offsetX) {
  // Display comprehensive Day-Ahead analytics
  // Startet bei y=40 statt 85 - der redundante "Aktueller Preis"-Block (steht
  // bereits auf dem Home-Screen) wurde entfernt, um dem Chart unten mehr
  // garantierten Platz zu verschaffen.
  int yPos = 40;
  const int LINE_HEIGHT = 15;
  const int INDENT = Layout::PADDING_LARGE + offsetX;

  // Date and data quality
  tft.setTextColor(Colors::TEXT_LABEL);
  char headerText[64];
  snprintf(headerText, sizeof(headerText), "%s (Qualitaet: %d%%)",
           appState.dayAheadPrices.date, appState.dayAheadPrices.dataQuality);
  tft.drawString(headerText, INDENT, yPos, 1);
  yPos += LINE_HEIGHT;

  // Price statistics
  tft.setTextColor(Colors::TEXT_MAIN);
  char statsText[80];
  snprintf(statsText, sizeof(statsText), "Ø %.1fct  Min: %.1fct@%02d:00  Max: %.1fct@%02d:00",
           appState.dayAheadPrices.dailyAverage, appState.dayAheadPrices.minPrice, appState.dayAheadPrices.cheapestHour,
           appState.dayAheadPrices.maxPrice, appState.dayAheadPrices.expensiveHour);
  tft.drawString(statsText, INDENT, yPos, 1);
  yPos += LINE_HEIGHT + 5;

  // Trend and volatility
  uint16_t trendColor = (appState.dayAheadPrices.trend == TREND_RISING) ? Colors::STATUS_RED :
                       (appState.dayAheadPrices.trend == TREND_FALLING) ? Colors::STATUS_GREEN :
                       Colors::STATUS_BLUE;
  const char* trendText = (appState.dayAheadPrices.trend == TREND_RISING) ? "Steigend" :
                         (appState.dayAheadPrices.trend == TREND_FALLING) ? "Fallend" : "Stabil";

  tft.setTextColor(trendColor);
  char trendStr[60];
  snprintf(trendStr, sizeof(trendStr), "Trend: %s  Volatilität: %.1f%%",
           trendText, appState.dayAheadPrices.volatilityIndex);
  tft.drawString(trendStr, INDENT, yPos, 1);
  yPos += LINE_HEIGHT + 10;

  // Die "Optimale Zeiten"-Liste und die Einsparungs-Zeile wurden entfernt -
  // der Chart darunter zeigt Preisverlauf und günstige/teure Stunden bereits
  // visuell (Farbcodierung + Ø-Linie), die Textliste war redundant und hat
  // dem Chart den Platz am unteren Bildschirmrand streitig gemacht.

  // Simple price chart/visualization – fest am unteren Bildschirmrand verankert,
  // damit variabler Content darüber den Chart nie aus dem sichtbaren Bereich
  // schiebt (Issue #2 hatte dieses Muster bereits für den Anti-Burnin-Offset gezeigt).
  const int CHART_HEIGHT = 70;
  const int CHART_BOTTOM_MARGIN = 17; // Platz für Zeitlabels (12) + Update-Zeile (15), leicht überlappend im Zeilenabstand
  int chartY = Layout::DISPLAY_HEIGHT - CHART_BOTTOM_MARGIN - CHART_HEIGHT - 2;
  chartY = max(chartY, yPos + 5); // nicht über den Text darüber springen, falls viel Platz frei ist
  int chartWidth = min(280, 320 - INDENT - 10); // Ensure chart fits on screen
  drawSimplePriceChart(INDENT, chartY, chartWidth, CHART_HEIGHT);

  // Update timestamp direkt unter dem Chart
  if (appState.dayAheadPrices.lastUpdate > 0) {
    unsigned long age = (millis() - appState.dayAheadPrices.lastUpdate) / 1000;
    char ageText[32];
    if (age < 60) {
      snprintf(ageText, sizeof(ageText), "Update vor: %lus", age);
    } else if (age < 3600) {
      snprintf(ageText, sizeof(ageText), "Update vor: %lum", age / 60);
    } else {
      snprintf(ageText, sizeof(ageText), "Update vor: %luh", age / 3600);
    }
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString(ageText, INDENT, chartY + CHART_HEIGHT + 12, 1);
  }
}

void drawSimplePriceChart(int x, int y, int width, int height) {
  // Draw a simple bar chart showing 24h price distribution with color coding
  if (!appState.dayAheadPrices.hasData) return;

  const int BAR_WIDTH = width / 24;
  const int CHART_HEIGHT = height - 10; // Leave space for time labels

  // Find valid price range for scaling
  float minVal = appState.dayAheadPrices.minPrice;
  float maxVal = appState.dayAheadPrices.maxPrice;
  if (maxVal <= minVal) return;

  // Aktuelle Stunde einmal vorab bestimmen statt pro Balken neu
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  int currentHour = timeinfo ? timeinfo->tm_hour : -1;

  // Ø-Preis-Linie vor den Balken zeichnen (dezent, im Hintergrund)
  float avgPrice = appState.dayAheadPrices.dailyAverage;
  if (avgPrice > minVal && avgPrice < maxVal) {
    int avgY = y + CHART_HEIGHT - (int)((avgPrice - minVal) / (maxVal - minVal) * CHART_HEIGHT);
    for (int dx = 0; dx < width; dx += 4) {
      tft.drawFastHLine(x + dx, avgY, 2, Colors::TEXT_LABEL);
    }
  }

  // Draw price bars for each hour
  for (int hour = 0; hour < 24; hour++) {
    if (appState.dayAheadPrices.prices[hour].isValid) {
      int barX = x + (hour * BAR_WIDTH) + 1;
      int barW = BAR_WIDTH - 1;
      float price = appState.dayAheadPrices.prices[hour].price;
      int barHeight = (int)((price - minVal) / (maxVal - minVal) * CHART_HEIGHT);

      // Color based on price category
      uint16_t barColor;
      switch (appState.dayAheadPrices.prices[hour].category) {
        case PRICE_VERY_CHEAP:   barColor = Colors::STATUS_GREEN; break;
        case PRICE_CHEAP:        barColor = Colors::PRICE_CHEAP_GREEN; break;
        case PRICE_MEDIUM:       barColor = Colors::STATUS_BLUE; break;
        case PRICE_EXPENSIVE:    barColor = Colors::STATUS_ORANGE; break;
        case PRICE_VERY_EXPENSIVE: barColor = Colors::STATUS_RED; break;
        default:                 barColor = Colors::TEXT_LABEL; break;
      }

      // Balken mit abgerundeter Oberkante und dezentem Glanzstreifen
      if (barHeight > 0 && barW > 0) {
        int barY = y + CHART_HEIGHT - barHeight;
        int r = min(2, min(barW / 2, barHeight / 2));
        if (r > 0) {
          tft.fillRoundRect(barX, barY, barW, barHeight, r, barColor);
        } else {
          tft.fillRect(barX, barY, barW, barHeight, barColor);
        }
        if (barHeight > 2) {
          tft.drawFastHLine(barX + (r > 0 ? 1 : 0), barY + 1, barW - (r > 0 ? 2 : 0), lgfx::color565(255, 255, 255));
        }
      }

      // Aktuelle Stunde: gefüllter Marker oberhalb des Balkens statt dünnem Rahmen
      if (hour == currentHour) {
        int markerX = barX + barW / 2;
        int markerY = y - 1;
        tft.fillTriangle(markerX - 3, markerY - 4, markerX + 3, markerY - 4, markerX, markerY, Colors::TEXT_MAIN);
      }
    }
  }

  // Chart-Rahmen zuletzt, damit er über den Balken liegt
  tft.drawRect(x, y, width, height, Colors::BORDER_MAIN);

  // Draw time labels every 6 hours
  tft.setTextColor(Colors::TEXT_LABEL);
  for (int h = 0; h < 24; h += 6) {
    int labelX = x + (h * BAR_WIDTH);
    char timeStr[4];
    snprintf(timeStr, sizeof(timeStr), "%02d", h);
    tft.drawString(timeStr, labelX, y + height + 2, 1);
  }
}

void drawOekostromDetailScreen() {
  Serial.println("Zeichne Ökostrom-Detail-Screen...");

  tft.fillScreen(Colors::BG_MAIN);
  int offsetX = appState.antiBurnin.getOffsetX();
  int offsetY = appState.antiBurnin.getOffsetY();

  // Titel
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Oekostrom Details", 10 + offsetX, 10 + offsetY, 2);

  // Zurück-Button (oben rechts)
  int backButtonX = 270 + offsetX;
  int backButtonY = 10 + offsetY;
  tft.drawRoundRect(backButtonX, backButtonY, 40, 20, 3, Colors::BORDER_MAIN);
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Zurueck", backButtonX + 3, backButtonY + 6, 1);

  // Aktueller Ökostrom-Anteil (groß anzeigen)
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("Aktueller Anteil:", 10 + offsetX, 40, 2);

  if (!appState.sensors[0].isTimedOut) {
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString(appState.sensors[0].formattedValue, 10 + offsetX, 65, 4);

    // Status-Indikator entfernt - mehr Platz für Diagramm

  } else {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("--- %", 10 + offsetX, 65, 4);
    tft.drawString("Keine Verbindung", 10 + offsetX, 105, 2);
  }

  // Verbessertes Day-Ahead Preis-Diagramm (24h-Verlauf)
  if (appState.dayAheadPrices.hasData) {
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("Day-Ahead Preise (24h):", 10 + offsetX, 110, 1);

    const int chartX = 10 + offsetX;
    const int chartY = 130;
    const int chartWidth = 280;
    const int chartHeight = 50;

    // Rahmen für das Diagramm
    tft.drawRect(chartX, chartY, chartWidth, chartHeight, Colors::BORDER_MAIN);

    // Finde Min/Max für Skalierung (alle 24 Stunden)
    float minPrice = 999.0f;
    float maxPrice = -999.0f;
    int validCount = 0;

    for (int i = 0; i < 24; i++) {
      if (appState.dayAheadPrices.prices[i].isValid) {
        minPrice = min(minPrice, appState.dayAheadPrices.prices[i].price);
        maxPrice = max(maxPrice, appState.dayAheadPrices.prices[i].price);
        validCount++;
      }
    }

    if (validCount > 0) {
      float priceRange = maxPrice - minPrice;
      // Entferne Fix für zu kleine Werte - das führte zu Preissprüngen

      // Zeichne Preisverlauf als vertikale Balken
      const int barWidth = chartWidth / 24;

      for (int i = 0; i < 24; i++) {
        if (appState.dayAheadPrices.prices[i].isValid) {
          float price = appState.dayAheadPrices.prices[i].price;

          // Höhe des Balkens basierend auf Preis
          int barHeight = (int)((price - minPrice) / priceRange * (chartHeight - 4));
          barHeight = max(2, barHeight);

          // Farbe basierend auf Preis (relativ zu Min/Max)
          uint16_t barColor = Colors::STATUS_GREEN;
          if (price > (minPrice + priceRange * 0.75f)) {
            barColor = Colors::STATUS_RED;      // Teuer
          } else if (price > (minPrice + priceRange * 0.5f)) {
            barColor = Colors::STATUS_ORANGE;   // Mittel
          } else if (price > (minPrice + priceRange * 0.25f)) {
            barColor = Colors::STATUS_YELLOW;   // Günstig
          }
          // else: Sehr günstig = grün

          int barX = chartX + 2 + i * barWidth;
          int barY = chartY + chartHeight - 2 - barHeight;

          tft.fillRect(barX, barY, barWidth - 1, barHeight, barColor);

          // Stundenmarkierung alle 4 Stunden
          if (i % 4 == 0) {
            tft.setTextColor(Colors::TEXT_LABEL);
            char hourStr[4];
            snprintf(hourStr, sizeof(hourStr), "%02d", i);
            tft.drawString(hourStr, barX - 2, chartY + chartHeight + 2, 1);
          }
        }
      }

      // Durchschnittslinie
      float avgPrice = 0.0f;
      for (int i = 0; i < 24; i++) {
        if (appState.dayAheadPrices.prices[i].isValid) {
          avgPrice += appState.dayAheadPrices.prices[i].price;
        }
      }
      avgPrice /= validCount;

      int avgY = chartY + chartHeight - 2 - (int)((avgPrice - minPrice) / priceRange * (chartHeight - 4));
      tft.drawLine(chartX + 2, avgY, chartX + chartWidth - 2, avgY, Colors::TEXT_MAIN);

      // Preisinformationen
      char priceInfo[64];
      snprintf(priceInfo, sizeof(priceInfo), "Min: %.1fct  Ø: %.1fct  Max: %.1fct",
               minPrice, avgPrice, maxPrice);
      tft.setTextColor(Colors::TEXT_LABEL);
      tft.drawString(priceInfo, 10 + offsetX, 188, 1);

      // Aktuelle Stunde hervorheben (wenn Zeitdaten verfügbar)
      if (appState.systemStatus.timeValid) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          int currentHour = timeinfo.tm_hour;
          if (currentHour < 24 && appState.dayAheadPrices.prices[currentHour].isValid) {
            int currentX = chartX + 2 + currentHour * barWidth;
            tft.drawRect(currentX - 1, chartY + 1, barWidth + 1, chartHeight - 2, Colors::TEXT_MAIN);

            // Aktueller Preis anzeigen
            char currentPriceStr[16];
            snprintf(currentPriceStr, sizeof(currentPriceStr), "Jetzt: %.1fct",
                     appState.dayAheadPrices.prices[currentHour].price);
            tft.setTextColor(Colors::TEXT_MAIN);
            tft.drawString(currentPriceStr, 10 + offsetX, 200, 1);
          }
        }
      }

    } else {
      tft.setTextColor(Colors::TEXT_TIMEOUT);
      tft.drawString("Keine gültigen Preisdaten", chartX + 10, chartY + 20, 1);
    }
  } else {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("Keine Day-Ahead Daten verfuegbar", 10 + offsetX, 130, 1);
  }

  // System-Info (Position angepasst für neues Chart)
  // MQTT Topic Info entfernt um Platz zu schaffen
  tft.drawString("home/PV/Share_renewable", 85 + offsetX, 215, 1);

  Serial.println("Ökostrom-Detail-Screen vollständig gezeichnet");
}

void drawPriceChart(int offsetX) {
  // Einfaches Balkendiagramm für 24h Preise
  const int chartX = 10 + offsetX;
  const int chartY = 85;
  const int chartWidth = 300;
  const int chartHeight = 80;
  const int barWidth = chartWidth / Layout::CHART_HOURS;

  // Chart-Rahmen
  tft.drawRect(chartX, chartY, chartWidth, chartHeight, Colors::BORDER_MAIN);

  // Finde Min/Max Preise für Skalierung
  float minPrice = 999.0f;
  float maxPrice = -999.0f;
  int validPrices = 0;

  for (int i = 0; i < Layout::CHART_HOURS; i++) {
    if (appState.dayAheadPrices.prices[i].isValid) {
      minPrice = min(minPrice, appState.dayAheadPrices.prices[i].price);
      maxPrice = max(maxPrice, appState.dayAheadPrices.prices[i].price);
      validPrices++;
    }
  }

  if (validPrices == 0) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("Keine gueltigen", chartX + 10, chartY + 20, 1);
    tft.drawString("Preisdaten", chartX + 10, chartY + 35, 1);
    return;
  }

  // Min/Max Preisanzeige entfernt für bessere Sichtbarkeit

  // Zeichne Balken für jede Stunde
  for (int i = 0; i < Layout::CHART_HOURS; i++) {
    if (appState.dayAheadPrices.prices[i].isValid) {
      float price = appState.dayAheadPrices.prices[i].price;

      // Skalierung des Balkens
      float priceRange = maxPrice - minPrice;
      // Entferne Fix für zu kleine Werte - das führte zu Preissprüngen

      int barHeight = (priceRange > 0) ? (int)((price - minPrice) / priceRange * (chartHeight - 4)) : 1;
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
      if (i % Layout::CHART_HOUR_INTERVAL == 0) {
        tft.setTextColor(Colors::TEXT_LABEL);
        char hourStr[4];
        snprintf(hourStr, sizeof(hourStr), "%d", i);
        tft.drawString(hourStr, barX, chartY + chartHeight + 2, 1);
      }
    }
  }

  Serial.printf("Preis-Chart gezeichnet: %d gültige Preise (%.2f - %.2f ct)\n",
                validPrices, minPrice, maxPrice);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              ZONE E – STATUS + SMARTPLUG
// ═══════════════════════════════════════════════════════════════════════════════

void drawZoneE() {
  int ox = appState.antiBurnin.getOffsetX();
  // Zone E endet exakt an der unteren Displaykante (200+40=240) - positiver
  // Offset würde Inhalt aus dem sichtbaren Bereich schieben (Issue #2), daher nach oben clampen.
  int oy = min(0, appState.antiBurnin.getOffsetY());

  const int ZE_Y = 200;
  const int ZE_H = 40;

  // Hintergrund löschen
  tft.fillRect(max(0, ox), ZE_Y + oy, 320 - abs(ox), ZE_H, Colors::BG_MAIN);

  // ── Kalender: nächster Termin, eine Zeile über der System-Info ───────────
  if (!appState.calendar.isStale()) {
    int calX = 2 + ox;
    int calY = ZE_Y + ZE_H - 22 + oy;
    // Auf Displaybreite kuerzen: lange Titel wuerden sonst die Uhrzeit am Ende
    // abschneiden bzw. aus dem Bild laufen. Uhrzeit ("HH:MM") bleibt erhalten.
    char calText[64];
    strncpy(calText, appState.calendar.nextEventText, sizeof(calText) - 1);
    calText[sizeof(calText) - 1] = '\0';
    const int maxW = 316 - calX;
    tft.setTextFont(1);  // textWidth() nutzt den aktiven Font (LovyanGFX)
    if (tft.textWidth(calText) > maxW) {
      char* sp = strrchr(calText, ' ');  // letztes Wort = Uhrzeit
      char timePart[16] = "";
      if (sp) {
        strncpy(timePart, sp, sizeof(timePart) - 1);
        timePart[sizeof(timePart) - 1] = '\0';
        *sp = '\0';
      }
      size_t len = strlen(calText);
      char shortened[64];
      while (len > 1) {
        len--;
        snprintf(shortened, sizeof(shortened), "%.*s...%s", (int)len, calText, timePart);
        if (tft.textWidth(shortened) <= maxW) break;
      }
      strncpy(calText, shortened, sizeof(calText) - 1);
      calText[sizeof(calText) - 1] = '\0';
    }
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString(calText, calX, calY, 1);
  }

  // ── Linke Spalte: System-Info auf einer Zeile, am unteren Zonenrand ───────
  int infoX = 2 + ox;
  int infoY = ZE_Y + ZE_H - 10 + oy;

  char infoLine[80];
  char uptimeText[16];
  if (appState.systemStatus.uptime < 3600)
    snprintf(uptimeText, sizeof(uptimeText), "%lum", appState.systemStatus.uptime / 60);
  else
    snprintf(uptimeText, sizeof(uptimeText), "%luh", appState.systemStatus.uptime / 3600);

  snprintf(infoLine, sizeof(infoLine), "RAM:%luK Up:%s RSSI:%ddBm",
           appState.systemStatus.freeHeap / 1024, uptimeText, appState.systemStatus.wifiRSSI);

  if (appState.systemStatus.lowMemoryWarning) {
    tft.setTextColor(Colors::SYSTEM_WARN);
  } else {
    tft.setTextColor(Colors::TEXT_LABEL);
  }
  tft.drawString(infoLine, infoX, infoY, 1);

  // Plug-Button und LDR wurden in Zone D integriert bzw. entfernt
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              SENSOR UND UI-KOMPONENTEN
// ═══════════════════════════════════════════════════════════════════════════════

bool isStockDisplayTime() {
  // Prüft ob die aktuelle Zeit zwischen 8:00 und 22:00 liegt
  if (!appState.systemStatus.timeValid) return true; // Fallback: zeige an wenn Zeit nicht verfügbar
  
  // Sichere Parse der aktuellen Uhrzeit (Format: "HH:MM:SS")
  if (strlen(appState.systemStatus.currentTime) < 5) return true; // Fallback bei ungültiger Zeit
  
  int hour = 0, minute = 0;
  int parsed = sscanf(appState.systemStatus.currentTime, "%d:%d", &hour, &minute);
  
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
  
  const SensorData& sensor1 = appState.sensors[index1];
  const SensorData& sensor2 = appState.sensors[index2];
  int offsetX = appState.antiBurnin.getOffsetX();
  int offsetY = appState.antiBurnin.getOffsetY();

  int boxX = x + offsetX;
  int boxY = y + offsetY;
  
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

// ═══════════════════════════════════════════════════════════════════════════════
//                    ZONE D – ICONS FÜR TEMPERATUR/SPEICHER-BOXEN
// ═══════════════════════════════════════════════════════════════════════════════
// Kleine Vektor-Icons (~14x14px) aus Grundformen, passend zum bestehenden Stil
// (Zone C nutzt ebenfalls nur geometrische Formen statt Bitmap-Icons).

// Sonne: Kreis mit 4 Strahlen - für Außentemperatur
static void drawSunIcon(int cx, int cy, uint16_t color) {
  tft.fillCircle(cx, cy, 4, color);
  tft.drawFastVLine(cx, cy - 7, 3, color);
  tft.drawFastVLine(cx, cy + 4, 3, color);
  tft.drawFastHLine(cx - 7, cy, 3, color);
  tft.drawFastHLine(cx + 4, cy, 3, color);
  tft.drawLine(cx - 5, cy - 5, cx - 3, cy - 3, color);
  tft.drawLine(cx + 3, cy - 3, cx + 5, cy - 5, color);
  tft.drawLine(cx - 5, cy + 5, cx - 3, cy + 3, color);
  tft.drawLine(cx + 3, cy + 3, cx + 5, cy + 5, color);
}

// Wassertropfen - für Wassertemperatur
static void drawDropIcon(int cx, int cy, uint16_t color) {
  tft.fillCircle(cx, cy + 2, 5, color);
  tft.fillTriangle(cx - 4, cy, cx + 4, cy, cx, cy - 8, color);
}

// Haus-Silhouette - für Hausspeicher
static void drawHouseIcon(int cx, int cy, uint16_t color) {
  tft.fillTriangle(cx - 7, cy - 1, cx + 7, cy - 1, cx, cy - 8, color);
  tft.fillRect(cx - 5, cy - 1, 10, 8, color);
}

// Auto-Silhouette (Seitenansicht, vereinfacht) - für PKW-Ladestand
static void drawCarIcon(int cx, int cy, uint16_t color) {
  tft.fillRoundRect(cx - 8, cy - 3, 16, 6, 2, color);
  tft.fillRoundRect(cx - 4, cy - 7, 8, 5, 1, color);
  tft.fillCircle(cx - 5, cy + 4, 2, color);
  tft.fillCircle(cx + 5, cy + 4, 2, color);
}

// ── Zone D links: Außentemp (oben) + Wassertemp (unten), Icon-Spalte links ──
void drawTempCombinedBox(int x, int y, int width, int height) {
  int offsetX = appState.antiBurnin.getOffsetX();
  int offsetY = appState.antiBurnin.getOffsetY();
  int boxX = x + offsetX;
  int boxY = y + offsetY;

  const SensorData& outside = appState.sensors[6];
  const SensorData& water = appState.sensors[7];

  uint16_t bgColor;
  if (outside.isTimedOut && water.isTimedOut) {
    bgColor = Colors::BG_BOX_TIMEOUT;
  } else {
    bgColor = getRowBackgroundColor(6);
  }
  tft.fillRoundRect(boxX, boxY, width, height, Layout::SENSOR_BOX_RADIUS, bgColor);
  tft.drawRoundRect(boxX, boxY, width, height, Layout::SENSOR_BOX_RADIUS, Colors::BORDER_MAIN);

  const int iconColX = boxX + 12;
  const int rowH = height / 2;
  const int textX = boxX + 26;

  // Obere Reihe: Außentemp
  drawSunIcon(iconColX, boxY + rowH / 2, Colors::STATUS_YELLOW);
  tft.setTextColor(outside.isTimedOut ? Colors::TEXT_TIMEOUT : Colors::TEXT_MAIN);
  char outsideBuf[16];
  snprintf(outsideBuf, sizeof(outsideBuf), outside.isTimedOut ? "---" : "%.1f C", outside.value);
  tft.drawString(outsideBuf, textX, boxY + rowH / 2 - 9, &fonts::DejaVu18);

  // Trennlinie zwischen den beiden Reihen
  tft.drawFastHLine(boxX + 4, boxY + rowH, width - 8, Colors::BORDER_INDICATOR);

  // Untere Reihe: Wassertemp
  drawDropIcon(iconColX, boxY + rowH + rowH / 2, Colors::STATUS_CYAN);
  tft.setTextColor(water.isTimedOut ? Colors::TEXT_TIMEOUT : Colors::TEXT_MAIN);
  char waterBuf[16];
  snprintf(waterBuf, sizeof(waterBuf), water.isTimedOut ? "---" : "%.1f C", water.value);
  tft.drawString(waterBuf, textX, boxY + rowH + rowH / 2 - 9, &fonts::DejaVu18);

  // Legionellen-Warnindikator für Wassertemp (wie zuvor bei sensors[7])
  if (!water.isTimedOut && water.layout.hasIndicator) {
    uint16_t col;
    if (water.value >= 50.0f)      col = Colors::STATUS_GREEN;
    else if (water.value >= 48.0f) col = Colors::STATUS_YELLOW;
    else if (water.value >= 45.0f) col = Colors::STATUS_ORANGE;
    else                            col = Colors::STATUS_RED;
    drawIndicator(boxX + width - 10, boxY + rowH + rowH / 2, col, true);
  }
}

// ── Zone D rechts: Hausspeicher (oben) + PKW-Ladestand (unten), Icon-Spalte links ──
void drawStorageCombinedBox(int x, int y, int width, int height) {
  int offsetX = appState.antiBurnin.getOffsetX();
  int offsetY = appState.antiBurnin.getOffsetY();
  int boxX = x + offsetX;
  int boxY = y + offsetY;

  const SensorData& storage = appState.sensors[3];

  tft.fillRoundRect(boxX, boxY, width, height, Layout::SENSOR_BOX_RADIUS,
                    storage.isTimedOut ? Colors::BG_BOX_TIMEOUT : getRowBackgroundColor(3));
  tft.drawRoundRect(boxX, boxY, width, height, Layout::SENSOR_BOX_RADIUS, Colors::BORDER_MAIN);

  const int iconColX = boxX + 12;
  const int rowH = height / 2;
  const int textX = boxX + 26;
  const int barX = textX;
  const int barWidth = boxX + width - barX - Layout::PADDING_SMALL;

  // Obere Reihe: Hausspeicher (echter Sensor, siehe Issue #58 für Kontext)
  drawHouseIcon(iconColX, boxY + rowH / 2, Colors::TEXT_MAIN);
  if (storage.isTimedOut) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("---", textX, boxY + 3, 1);
  } else {
    uint16_t barColor = (storage.value >= 80.0f) ? Colors::STATUS_GREEN :
                        (storage.value >= 40.0f) ? Colors::STATUS_YELLOW :
                                                    Colors::STATUS_RED;
    char storBuf[8];
    snprintf(storBuf, sizeof(storBuf), "%.0f%%", storage.value);
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString(storBuf, textX, boxY + 2, 1);
    drawProgressBar(barX, boxY + rowH / 2 + 2, barWidth, storage.value, false, barColor);
  }

  // Trennlinie zwischen den beiden Reihen
  tft.drawFastHLine(boxX + 4, boxY + rowH, width - 8, Colors::BORDER_INDICATOR);

  // Untere Reihe: PKW-Ladestand aus TeslaMate (appState.car). Gleicher Aufbau wie
  // die Hausspeicher-Reihe: Prozent oben, Balken darunter.
  const CarState& car = appState.car;
  if (!car.hasValue) {
    drawCarIcon(iconColX, boxY + rowH + rowH / 2, Colors::TEXT_TIMEOUT);
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("---", textX, boxY + rowH + 3, 1);
  } else {
    bool stale = car.isStale();
    drawCarIcon(iconColX, boxY + rowH + rowH / 2, stale ? Colors::TEXT_TIMEOUT : Colors::TEXT_MAIN);
    char carBuf[16];
    snprintf(carBuf, sizeof(carBuf), car.isCharging() ? "%.0f%% laedt" : "%.0f%%", car.batteryLevel);
    tft.setTextColor(stale ? Colors::TEXT_TIMEOUT : Colors::TEXT_MAIN);
    tft.drawString(carBuf, textX, boxY + rowH + 3, 1);
    drawProgressBar(barX, boxY + rowH + rowH / 2 + 2, barWidth, car.batteryLevel, false,
                    stale ? Colors::TEXT_TIMEOUT : Colors::STORAGE_EV);
  }
}

void drawSensorBox(int index) {
  if (index < 0 || index >= System::SENSOR_COUNT) return;

  const SensorData& sensor = appState.sensors[index];

  // Sensoren ohne eigene Box (in Zone C integriert) überspringen
  if (sensor.layout.w == 0 || sensor.layout.h == 0) return;

  int ox = appState.antiBurnin.getOffsetX();
  int oy = appState.antiBurnin.getOffsetY();
  int boxX = sensor.layout.x + ox;
  int boxY = sensor.layout.y + oy;
  int bw = sensor.layout.w;
  int bh = sensor.layout.h;


  // Hintergrundfarbe – intern berechnete Sensoren nie auf Timeout-Farbe setzen
  uint16_t bgColor;
  if (index == 4) {
    bgColor = getRowBackgroundColor(index);
  } else {
    bgColor = sensor.isTimedOut ? Colors::BG_BOX_TIMEOUT : getRowBackgroundColor(index);
  }

  tft.fillRoundRect(boxX, boxY, bw, bh, Layout::SENSOR_BOX_RADIUS, bgColor);
  tft.drawRoundRect(boxX, boxY, bw, bh, Layout::SENSOR_BOX_RADIUS, Colors::BORDER_MAIN);

  // ── Index 1: Preis-Box (groß, Zone B links) ──────────────────────────────
  if (index == 1) {
    if (!sensor.isTimedOut) {
      uint16_t priceBg = Colors::BG_ROW1;
      if (sensor.value < 15.0f)       priceBg = 0x0320;
      else if (sensor.value < 25.0f)  priceBg = 0x0240;
      else if (sensor.value > 40.0f)  priceBg = 0x4000;
      tft.fillRoundRect(boxX, boxY, bw, bh, Layout::SENSOR_BOX_RADIUS, priceBg);
      tft.drawRoundRect(boxX, boxY, bw, bh, Layout::SENSOR_BOX_RADIUS, Colors::BORDER_MAIN);
    }
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("Preis", boxX + Layout::PADDING_SMALL, boxY + Layout::PADDING_SMALL, 1);
    tft.setTextColor(sensor.isTimedOut ? Colors::TEXT_TIMEOUT : Colors::TEXT_MAIN);
    char priceBuf[20];
    if (sensor.isTimedOut) {
      snprintf(priceBuf, sizeof(priceBuf), "---");
    } else {
      snprintf(priceBuf, sizeof(priceBuf), "%.1f ct", sensor.value);
    }
    tft.drawString(priceBuf, boxX + Layout::PADDING_SMALL, boxY + 14, &fonts::DejaVu18);
    return;
  }

  // ── Index 0: Ökostrom (Zone B rechts, gleichgroß wie Preis) ─────────────
  if (index == 0) {
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("Oekostrom", boxX + Layout::PADDING_SMALL, boxY + Layout::PADDING_SMALL, 1);
    tft.setTextColor(sensor.isTimedOut ? Colors::TEXT_TIMEOUT : Colors::TEXT_MAIN);
    char ecoBuf[20];
    if (sensor.isTimedOut) {
      snprintf(ecoBuf, sizeof(ecoBuf), "---");
    } else {
      snprintf(ecoBuf, sizeof(ecoBuf), "%.1f%%", sensor.value);
    }
    tft.drawString(ecoBuf, boxX + Layout::PADDING_SMALL, boxY + 14, &fonts::DejaVu18);
    return;
  }

  // Index 3 (Speicher) und 6/7 (Temperaturen) werden nicht mehr über die generische
  // Box gezeichnet - siehe drawStorageCombinedBox() / drawTempCombinedBox() für das
  // platzsparende, zweigeteilte Zone-D-Layout mit Icons.

  // ── Fallback: Label + Wert (für alle anderen Sensoren) ───────────────────
  tft.setTextColor(sensor.isTimedOut ? Colors::TEXT_TIMEOUT : Colors::TEXT_LABEL);
  tft.drawString(sensor.label, boxX + Layout::PADDING_SMALL, boxY + Layout::PADDING_SMALL, 1);

  if (sensor.isTimedOut) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("---", boxX + Layout::PADDING_SMALL, boxY + 15, 2);
  } else {
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString(sensor.formattedValue, boxX + Layout::PADDING_SMALL, boxY + 15, 2);
    if (index != 6) {
      drawTrendArrow(boxX + bw - 20, boxY + Layout::PADDING_SMALL + 5, sensor.trend, index);
    }
    if (sensor.layout.hasProgressBar) {
      uint16_t barColor = getSensorProgressColor(index);
      drawProgressBar(boxX + Layout::PADDING_SMALL,
                      boxY + bh - 8,
                      bw - 2 * Layout::PADDING_SMALL,
                      sensor.value, false, barColor);
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
    
    case 2: // Aktienkurs: Vergleich mit appState.stockPreviousClose
      if (appState.stockPreviousClose <= 0.0f) return Colors::TEXT_MAIN;
      return (appState.sensors[index].value > appState.stockPreviousClose) ? Colors::TREND_UP : 
             (appState.sensors[index].value < appState.stockPreviousClose) ? Colors::TREND_DOWN : Colors::TEXT_MAIN;
    
    default: // Standard: höhere Werte = grün
      return (trend == SensorData::UP) ? Colors::TREND_UP : 
             (trend == SensorData::DOWN) ? Colors::TREND_DOWN : Colors::TEXT_MAIN;
  }
}

void drawSystemInfo() {
  // System-Info ist jetzt in drawZoneE() integriert – kein eigenständiger Zeichenaufruf nötig.
  // Funktion bleibt für selektive Redraws aus updateDisplay() erhalten.
  drawZoneE();
}

// Grenze in Zone A zwischen Uhrzeit/Datum (links) und Netzstatus (rechts).
// Der Status beginnt bei x=190, Uhrzeit (Font 2) und Datum enden deutlich davor.
static constexpr int ZONE_A_SPLIT_X = 186;

void drawNetworkStatus() {
  // Zone A rechts: WiFi  MQTT  OTA als farbige Abkürzungen (y=0..27), keine Kreise
  int ox = appState.antiBurnin.getOffsetX();
  // Wie drawTimeDisplay(): Zone A grenzt an y=0, negativer Offset nach unten clampen
  int oy = max(0, appState.antiBurnin.getOffsetY());

  const int BASE_X = 190 + ox;
  const int BASE_Y = 2 + oy;
  const int STEP = 42;

  // Nur den eigenen Bereich löschen (Zone A rechts). drawTimeDisplay() löscht
  // umgekehrt nur die linke Hälfte - sonst radiert jeder Uhrzeit-Tick den Status aus.
  tft.fillRect(ZONE_A_SPLIT_X + ox, oy, 320 - (ZONE_A_SPLIT_X + ox), 27, Colors::BG_MAIN);

  // WiFi
  uint16_t wifiCol = appState.systemStatus.wifiConnected ?
    getRSSIColor(appState.systemStatus.wifiRSSI) : Colors::STATUS_RED;
  tft.setTextColor(wifiCol);
  tft.drawString("WiFi", BASE_X, BASE_Y, 1);

  // MQTT
  uint16_t mqttCol = appState.systemStatus.mqttConnected ? Colors::STATUS_GREEN : Colors::STATUS_RED;
  tft.setTextColor(mqttCol);
  tft.drawString("MQTT", BASE_X + STEP, BASE_Y, 1);

  // OTA
  uint16_t otaCol = isOTAActive() ? Colors::STATUS_YELLOW : Colors::TEXT_TIMEOUT;
  tft.setTextColor(otaCol);
  tft.drawString("OTA", BASE_X + 2*STEP, BASE_Y, 1);
}


void drawTimeDisplay() {
  // Zone A: Zeit links (0..~160), Netzstatus rechts (160..320)
  int ox = appState.antiBurnin.getOffsetX();
  // Zone A grenzt an die obere Displaykante (y=0) - negativer Offset würde
  // Inhalt aus dem sichtbaren Bereich schieben (Issue #2), daher nach unten clampen.
  int oy = max(0, appState.antiBurnin.getOffsetY());

  // Nur den Uhrzeit-Bereich löschen (links von ZONE_A_SPLIT_X). Früher wurde die
  // ganze Zone-A-Breite gelöscht - das hat bei jedem Sekundenwechsel WiFi/MQTT/OTA
  // (rechts, nur bei Verbindungsänderung neu gezeichnet) mit ausradiert.
  tft.fillRect(max(0, ox), oy, ZONE_A_SPLIT_X + ox - max(0, ox), 27, Colors::BG_MAIN);

  if (appState.systemStatus.timeValid) {
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString(appState.systemStatus.currentTime, 2 + ox, 1 + oy, 2);
    tft.setTextColor(Colors::TEXT_LABEL);
    // Wochentag + Datum
    struct tm timeinfo;
    char dateBuf[20];
    if (getLocalTime(&timeinfo)) {
      const char* weekdays[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
      snprintf(dateBuf, sizeof(dateBuf), "%s %s", weekdays[timeinfo.tm_wday], appState.systemStatus.currentDate);
    } else {
      strncpy(dateBuf, appState.systemStatus.currentDate, sizeof(dateBuf) - 1);
    }
    tft.drawString(dateBuf, 2 + ox, 16 + oy, 1);
  } else {
    tft.setTextColor(Colors::SYSTEM_ERROR);
    tft.drawString("--:--:--", 2 + ox, 1 + oy, 2);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              ECO-VISUALISIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

void drawEcoVisualization(int x, int y) {
  if (appState.loadPower < PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    // Kein Verbrauch - zeige Standby-Symbol
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("~", x, y, 2);
    return;
  }
  
  // Berechne Eco-Score basierend auf Energiequellen
  float totalConsumption = appState.loadPower;
  float pvDirectUse = min(appState.pvPower, totalConsumption);
  float batteryUse = (!appState.isStorageCharging && appState.storagePower > 0) ? 
                     min(appState.storagePower, totalConsumption - pvDirectUse) : 0.0f;
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
//                              ZENTRALE FARBVERWALTUNG
// ═══════════════════════════════════════════════════════════════════════════════

uint16_t getSensorProgressColor(int sensorIndex) {
  switch (sensorIndex) {
    case 3:  // Ladestand (Akku/Speicher)
      return Colors::STATUS_BLUE;
    case 5:  // PV-Erzeugung
      return Colors::STATUS_GREEN;
    default:
      return 0; // Use percentage-based default colors
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              BASIS-UI-ELEMENTE
// ═══════════════════════════════════════════════════════════════════════════════

void drawProgressBar(int x, int y, int width, float percentage, bool showText, uint16_t customColor) {
  tft.drawRect(x, y, width, Layout::PROGRESS_BAR_HEIGHT, Colors::BORDER_PROGRESS);
  tft.fillRect(x + 1, y + 1, width - 2, Layout::PROGRESS_BAR_HEIGHT - 2, Colors::BG_MAIN);

  int progressWidth = constrain((width - 2) * (percentage / 100.0f), 0, width - 2);
  uint16_t progressColor;

  if (customColor != 0) {
    // Use custom color if provided
    progressColor = customColor;
  } else {
    // Default color scheme based on percentage (for generic progress bars)
    if (percentage >= 75) progressColor = Colors::STATUS_GREEN;      // >= 75%: Grün (gut)
    else if (percentage >= 50) progressColor = Colors::STATUS_YELLOW; // 50-74%: Gelb
    else if (percentage >= 25) progressColor = Colors::STATUS_ORANGE; // 25-49%: Orange
    else progressColor = Colors::STATUS_RED;                         // < 25%: Rot (schlecht)
  }
  
  if (progressWidth > 0) {
    tft.fillRect(x + 1, y + 1, progressWidth, Layout::PROGRESS_BAR_HEIGHT - 2, progressColor);
  }
  
  if (showText) {
    tft.setTextColor(Colors::TEXT_MAIN);
    char percentageText[8];
    snprintf(percentageText, sizeof(percentageText), "%.0f%%", percentage);
    tft.drawString(percentageText, x + width + Layout::PADDING_MEDIUM, y - 2, 1);
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
    if (appState.sensors[index].lastValue > 0) {
      changePercent = abs(appState.sensors[index].value - appState.sensors[index].lastValue) / appState.sensors[index].lastValue * 100.0f;
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
    
    case 2: // Aktien: Vergleich mit appState.stockPreviousClose für besondere Behandlung
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
  float totalConsumption = max(0.0f, appState.loadPower); // Negative Werte abfangen
  
  if (totalConsumption < PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    // Sehr geringer Verbrauch - zeige leeren Balken
    tft.drawRect(x, y, width, barHeight, Colors::BORDER_PROGRESS);
    return;
  }
  
  // Robuste Energiequellen-Berechnung mit Validierung
  float pvDirectUse = max(0.0f, min(max(0.0f, appState.pvPower), totalConsumption));
  float remainingConsumption = max(0.0f, totalConsumption - pvDirectUse);
  
  float batteryUse = 0.0f;
  float gridUse = 0.0f;
  
  if (remainingConsumption > PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    if (!appState.isStorageCharging && appState.storagePower > PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
      // Batterie entlädt - sichere Berechnung
      batteryUse = max(0.0f, min(appState.storagePower, remainingConsumption));
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
  
  // Batterie-Segment (blau)
  if (batteryWidth > 0 && currentX < x + width) {
    int segmentWidth = min(batteryWidth, (x + width) - currentX);
    tft.fillRect(currentX, y, segmentWidth, barHeight, Colors::STATUS_BLUE);
    currentX += batteryWidth;
  }
  
  // Netz-Segment (rot)
  if (gridWidth > 0 && currentX < x + width) {
    int segmentWidth = min(gridWidth, (x + width) - currentX);
    tft.fillRect(currentX, y, segmentWidth, barHeight, Colors::STATUS_RED);
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
    
    if (!appState.isGridFeedIn) {
      // Bezug - Unterscheidung zwischen Netz (rot) und Speicher (blau)
      uint16_t bezugColor = Colors::STATUS_RED;  // Default: Netzbezug
      const char* bezugText = "Netz";

      // Prüfen ob Energie aus Speicher kommt (wenn Speicherleistung negativ ist)
      if (appState.storagePower > 0.1f && !appState.isStorageCharging) {
        bezugColor = Colors::STATUS_BLUE;  // Speicherbezug
        bezugText = "Speicher";
      }

      tft.fillRect(centerX - gridBarWidth, y, gridBarWidth, barHeight, bezugColor);
      tft.setTextColor(bezugColor);
      tft.drawString(bezugText, x + 2, y - 8, 1);

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
                      (appState.isGridFeedIn ? "[EINSPEISUNG]" : "[BEZUG]");
  Serial.printf("🔄 Grid-Balken: %.1fkW %s (PV=%.1f, Load=%.1f, Storage=%.1f)\n",
               gridPower, status, pvPower, appState.loadPower, appState.storagePower);
}

void drawPVDistributionBar(int x, int y, int width, float pvPower) {
  // Segmentierte Progress Bar für PV-Erzeugung Aufteilung
  // Grün: Strom ins Auto (Wallbox), Blau: Strom in Hausspeicher, Rot: Strom ins Netz

  const int MIN_SEGMENT_WIDTH = 3;
  int barHeight = Layout::PROGRESS_BAR_HEIGHT;

  // Hintergrund löschen
  tft.fillRect(x, y - 1, width, barHeight + 2, Colors::BG_MAIN);

  if (pvPower < PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    // Keine PV-Erzeugung - leerer Balken
    tft.drawRect(x, y, width, barHeight, Colors::BORDER_PROGRESS);
    return;
  }

  // Berechne Energieaufteilung der PV-Erzeugung
  float totalPV = max(0.0f, pvPower);

  // 1. Priorität: Direkter Hausverbrauch (nicht in der Bar, da das die Basis ist)
  float directUse = min(totalPV, max(0.0f, appState.loadPower));
  float remainingPV = max(0.0f, totalPV - directUse);

  // 2. Priorität: Wallbox laden (echte appState.wallboxPower verwenden)
  float toWallbox = 0.0f;
  if (remainingPV > 0 && appState.wallboxPower > PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    // Verwende echte Wallbox-Leistung, aber maximal so viel wie noch übrig ist
    toWallbox = min(remainingPV, appState.wallboxPower);
    remainingPV = max(0.0f, remainingPV - toWallbox);
  }

  // 3. Priorität: Speicher laden
  float toStorage = 0.0f;
  if (remainingPV > 0 && appState.isStorageCharging && appState.storagePower > PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    toStorage = min(remainingPV, appState.storagePower);
    remainingPV = max(0.0f, remainingPV - toStorage);
  }

  // 4. Rest: Netzeinspeisung
  float toGrid = remainingPV;

  // Skalierung auf Balkenbreite
  float maxPV = PowerManagement::MAX_PV_POWER; // 30kW aus config.h
  float pvRatio = min(totalPV / maxPV, 1.0f);
  int totalBarWidth = max(1, (int)(width * pvRatio));

  // Berechne Segmentbreiten
  int wallboxWidth = 0, storageWidth = 0, gridWidth = 0;

  if (totalBarWidth > 0 && totalPV > PowerManagement::MIN_CONSUMPTION_THRESHOLD) {
    float wallboxRatio = (totalPV > 0) ? (toWallbox / totalPV) : 0.0f;
    float storageRatio = (totalPV > 0) ? (toStorage / totalPV) : 0.0f;
    float gridRatio = (totalPV > 0) ? (toGrid / totalPV) : 0.0f;

    // Pixel-Breiten mit Mindestbreiten
    wallboxWidth = (toWallbox > PowerManagement::MIN_CONSUMPTION_THRESHOLD) ?
                   max((int)(wallboxRatio * totalBarWidth), MIN_SEGMENT_WIDTH) : 0;
    storageWidth = (toStorage > PowerManagement::MIN_CONSUMPTION_THRESHOLD) ?
                   max((int)(storageRatio * totalBarWidth), MIN_SEGMENT_WIDTH) : 0;
    gridWidth = (toGrid > PowerManagement::MIN_CONSUMPTION_THRESHOLD) ?
                max((int)(gridRatio * totalBarWidth), MIN_SEGMENT_WIDTH) : 0;

    // Überlauf korrigieren
    if (wallboxWidth + storageWidth + gridWidth > totalBarWidth) {
      float scale = (float)totalBarWidth / (wallboxWidth + storageWidth + gridWidth);
      wallboxWidth = (int)(wallboxWidth * scale);
      storageWidth = (int)(storageWidth * scale);
      gridWidth = totalBarWidth - wallboxWidth - storageWidth;
    }
  }

  // Segmente zeichnen
  int currentX = x;

  // Wallbox-Segment (grün)
  if (wallboxWidth > 0) {
    tft.fillRect(currentX, y, wallboxWidth, barHeight, Colors::STATUS_GREEN);
    currentX += wallboxWidth;
  }

  // Speicher-Segment (blau/cyan)
  if (storageWidth > 0 && currentX < x + width) {
    int segmentWidth = min(storageWidth, (x + width) - currentX);
    tft.fillRect(currentX, y, segmentWidth, barHeight, Colors::STATUS_BLUE);
    currentX += storageWidth;
  }

  // Netz-Segment (rot)
  if (gridWidth > 0 && currentX < x + width) {
    int segmentWidth = min(gridWidth, (x + width) - currentX);
    tft.fillRect(currentX, y, segmentWidth, barHeight, Colors::STATUS_RED);
  }

  // Rahmen um verfügbaren Bereich
  tft.drawRect(x, y, totalBarWidth, barHeight, Colors::BORDER_PROGRESS);

  Serial.printf("🔋 PV-Distribution: %.1fkW (Wallbox:%.2f Speicher:%.2f Netz:%.2f) Breiten:(%d,%d,%d)\n",
                totalPV, toWallbox, toStorage, toGrid, wallboxWidth, storageWidth, gridWidth);
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
    appState.renderManager.markFullRedrawRequired();
    Serial.println("🔄 Touch-Marker Redraw ausgelöst");
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              NEUE SUBPAGE-SCREENS
// ═══════════════════════════════════════════════════════════════════════════════

void drawWallboxConsumptionScreen() {
  Serial.println("Zeichne Wallbox-Verbrauch-Screen...");

  tft.fillScreen(Colors::BG_MAIN);
  int offsetX = appState.antiBurnin.getOffsetX();
  int offsetY = appState.antiBurnin.getOffsetY();

  // Titel
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Wallbox Verbrauch", 10 + offsetX, 10 + offsetY, 2);

  // Zurück-Button (oben rechts)
  int backButtonX = 270 + offsetX;
  int backButtonY = 10 + offsetY;
  tft.drawRoundRect(backButtonX, backButtonY, 40, 20, 3, Colors::BORDER_MAIN);
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Zurueck", backButtonX + 3, backButtonY + 6, 1);

  // Wallbox Leistung (groß anzeigen) – echter Wert aus home/PV/WallboxPower
  // (appState.wallboxPower, kW). Früher wurde hier fälschlich sensors[5]
  // angezeigt, das ist die PV-Erzeugung.
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("Aktuelle Leistung:", 10 + offsetX, 40, 2);

  // Kein gueltiger Wert (nie empfangen oder laenger als der Sensor-Timeout her)
  // -> "Keine Verbindung" statt 0.0 kW / Standby bzw. veraltetem "Laden".
  unsigned long wbTime = appState.lastValidPower.wallboxPowerTime;
  bool wbTimedOut = (wbTime == 0) || (millis() - wbTime) > Timing::SENSOR_TIMEOUT_MS;
  if (wbTimedOut) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("Keine Verbindung", 10 + offsetX, 65, 2);
  } else {
    float wbPower = appState.wallboxPower;
    char wbText[16];
    snprintf(wbText, sizeof(wbText), "%.1f kW", wbPower);
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString(wbText, 10 + offsetX, 65, 4);

    uint16_t statusColor = Colors::STATUS_GREEN;
    const char* statusText = "Standby";
    if (wbPower > 0.5f) {
      statusColor = Colors::STATUS_BLUE;
      statusText = "Laden";
    } else if (wbPower > 0.1f) {
      statusColor = Colors::STATUS_YELLOW;
      statusText = "Bereit";
    }
    tft.setTextColor(statusColor);
    tft.drawString(statusText, 200 + offsetX, 75, 2);
  }

  // Auto-Ladestand (TeslaMate)
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("Ladestand Auto:", 10 + offsetX, 110, 1);
  const CarState& car = appState.car;
  if (!car.hasValue) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("nicht verfuegbar", 10 + offsetX, 125, 1);
  } else {
    bool stale = car.isStale();
    char carText[16];
    snprintf(carText, sizeof(carText), "%.0f%%", car.batteryLevel);
    tft.setTextColor(stale ? Colors::TEXT_TIMEOUT : Colors::TEXT_MAIN);
    tft.drawString(carText, 10 + offsetX, 125, 1);
    drawProgressBar(10 + offsetX, 138, 250, car.batteryLevel, false,
                    stale ? Colors::TEXT_TIMEOUT : Colors::STORAGE_EV);
  }

  // Hausspeicher-Ladestand – echter Sensorwert (sensors[3]), kein Simulationswert mehr
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("Hausspeicher:", 10 + offsetX, 150, 1);
  const SensorData& storage = appState.sensors[3];
  if (storage.isTimedOut) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("nicht verfuegbar", 10 + offsetX, 165, 1);
  } else {
    const char* storageStatusText = "Standby";
    uint16_t storageStatusColor = Colors::TEXT_MAIN;
    if (appState.storagePower > 0.1f) {
      if (appState.isStorageCharging) {
        storageStatusText = "Laden";
        storageStatusColor = Colors::STATUS_GREEN;
      } else {
        storageStatusText = "Entladen";
        storageStatusColor = Colors::STATUS_ORANGE;
      }
    }
    tft.setTextColor(storageStatusColor);
    char storageText[48];
    snprintf(storageText, sizeof(storageText), "%.0f%% (%s)", storage.value, storageStatusText);
    tft.drawString(storageText, 10 + offsetX, 165, 1);
    drawProgressBar(10 + offsetX, 180, 250, storage.value, false, Colors::STORAGE_HOUSE);
  }

  // System-Info unter den Progress-Bars (kürzer halten)
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("MQTT: home/PV/WallboxPower", 10 + offsetX, 200, 1);

  Serial.println("Wallbox-Verbrauch-Screen vollständig gezeichnet");
}

void drawLadestandScreen() {
  // Oben: Hausspeicher-Ladestand (sensors[3], MQTT-Topic home/PV/chargingLevel).
  // Unten: PKW-Ladestand aus TeslaMate (appState.car). Erreichbar über die
  // Speicher-Box in Zone D, die beide Werte enthält.
  Serial.println("Zeichne Ladestand-Screen...");

  tft.fillScreen(Colors::BG_MAIN);
  int offsetX = appState.antiBurnin.getOffsetX();
  int offsetY = appState.antiBurnin.getOffsetY();

  // Titel
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Ladestand", 10 + offsetX, 10 + offsetY, 2);

  // Zurück-Button (oben rechts)
  int backButtonX = 270 + offsetX;
  int backButtonY = 10 + offsetY;
  tft.drawRoundRect(backButtonX, backButtonY, 40, 20, 3, Colors::BORDER_MAIN);
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Zurueck", backButtonX + 3, backButtonY + 6, 1);

  // ── Hausspeicher (y=38..130) ────────────────────────────────────────────
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("Hausspeicher", 10 + offsetX, 38, 2);

  const SensorData& storage = appState.sensors[3];

  if (storage.isTimedOut) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("Nicht verfuegbar", 10 + offsetX, 62, 2);
  } else {
    // Status-Text basierend auf Lade-/Entlade-Zustand (echte Werte, kein Simulationswert)
    const char* storageStatusText = "Standby";
    uint16_t storageStatusColor = Colors::TEXT_MAIN;
    if (appState.storagePower > 0.1f) {
      if (appState.isStorageCharging) {
        storageStatusText = "Laden";
        storageStatusColor = Colors::STATUS_GREEN;
      } else {
        storageStatusText = "Entladen";
        storageStatusColor = Colors::STATUS_ORANGE;
      }
    }

    // Großer Prozent-Wert (echter Sensorwert), Status rechts daneben
    tft.setTextColor(storageStatusColor);
    char storageLevelText[8];
    snprintf(storageLevelText, sizeof(storageLevelText), "%.0f%%", storage.value);
    tft.drawString(storageLevelText, 10 + offsetX, 58, 4);
    tft.drawString(storageStatusText, 110 + offsetX, 64, 2);

    // Leistung anzeigen wenn aktiv
    if (appState.storagePower > 0.1f) {
      tft.setTextColor(Colors::TEXT_MAIN);
      char powerText[16];
      snprintf(powerText, sizeof(powerText), "%.1f kW", appState.storagePower);
      tft.drawString(powerText, 200 + offsetX, 64, 2);
    }

    drawProgressBar(10 + offsetX, 92, 280, storage.value, false, Colors::STORAGE_HOUSE);
  }

  // Trennlinie zwischen Hausspeicher und Auto
  tft.drawFastHLine(10 + offsetX, 112, 300, Colors::BORDER_INDICATOR);

  // ── Auto (y=120..230) ───────────────────────────────────────────────────
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("Auto", 10 + offsetX, 120, 2);

  const CarState& car = appState.car;

  if (!car.hasValue) {
    tft.setTextColor(Colors::TEXT_TIMEOUT);
    tft.drawString("Nicht verfuegbar", 10 + offsetX, 144, 2);
  } else {
    bool stale = car.isStale();
    uint16_t carColor = stale ? Colors::TEXT_TIMEOUT
                      : car.isCharging() ? Colors::STATUS_GREEN
                      : Colors::TEXT_MAIN;

    tft.setTextColor(carColor);
    char carLevelText[8];
    snprintf(carLevelText, sizeof(carLevelText), "%.0f%%", car.batteryLevel);
    tft.drawString(carLevelText, 10 + offsetX, 140, 4);

    // TeslaMate-State auf Deutsch (unbekannte Werte roh anzeigen)
    const char* stateText = car.state;
    if      (strcmp(car.state, "charging") == 0) stateText = "Laden";
    else if (strcmp(car.state, "asleep") == 0)   stateText = "Schlaeft";
    else if (strcmp(car.state, "online") == 0)   stateText = "Online";
    else if (strcmp(car.state, "driving") == 0)  stateText = "Faehrt";
    else if (strcmp(car.state, "offline") == 0)  stateText = "Offline";
    else if (strcmp(car.state, "updating") == 0) stateText = "Update";
    else if (strcmp(car.state, "suspended") == 0) stateText = "Ruht";
    tft.drawString(stateText, 110 + offsetX, 146, 2);

    // Wallbox-Leistung, solange geladen wird (echter Wert aus home/PV/WallboxPower)
    if (car.isCharging() && appState.wallboxPower > 0.05f) {
      tft.setTextColor(Colors::TEXT_MAIN);
      char wbText[16];
      snprintf(wbText, sizeof(wbText), "%.1f kW", appState.wallboxPower);
      tft.drawString(wbText, 200 + offsetX, 146, 2);
    }

    drawProgressBar(10 + offsetX, 174, 280, car.batteryLevel, false,
                    stale ? Colors::TEXT_TIMEOUT : Colors::STORAGE_EV);

    // Alter des Werts: Das Auto meldet im Schlaf nichts, der letzte Wert bleibt gültig
    unsigned long ageMin = (millis() - car.lastUpdate) / 60000UL;
    char ageText[40];
    if (ageMin < 1)        snprintf(ageText, sizeof(ageText), "empfangen: gerade eben");
    else if (ageMin < 120) snprintf(ageText, sizeof(ageText), "empfangen vor %lu min", ageMin);
    else                   snprintf(ageText, sizeof(ageText), "empfangen vor %lu h", ageMin / 60);
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString(ageText, 10 + offsetX, 190, 1);
  }

  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("MQTT: teslamate/cars/1/battery_level", 10 + offsetX, 222, 1);

  Serial.println("Ladestand-Screen vollständig gezeichnet");
}

void drawSettingsScreen() {
  Serial.println("Zeichne Settings-Screen...");

  tft.fillScreen(Colors::BG_MAIN);
  int offsetX = appState.antiBurnin.getOffsetX();
  int offsetY = appState.antiBurnin.getOffsetY();

  // Titel
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Einstellungen", 10 + offsetX, 10 + offsetY, 2);

  // Zurück-Button (oben rechts)
  int backButtonX = 270 + offsetX;
  int backButtonY = 10 + offsetY;
  tft.drawRoundRect(backButtonX, backButtonY, 40, 20, 3, Colors::BORDER_MAIN);
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Zurueck", backButtonX + 3, backButtonY + 6, 1);

  // Touch-Kalibrierung Sektion
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("Touch-Kalibrierung:", 10 + offsetX, 45, 2);

  // Kalibrierungs-Status
  bool hasCalibration = touchManager.hasValidCalibration();
  uint16_t statusColor = hasCalibration ? Colors::STATUS_GREEN : Colors::STATUS_ORANGE;
  const char* statusText = hasCalibration ? "Kalibriert" : "Nicht kalibriert";

  drawIndicator(10 + offsetX, 75, statusColor, true);
  tft.setTextColor(statusColor);
  tft.drawString(statusText, 25 + offsetX, 70, 1);

  // Kalibrierungs-Button
  int calibButtonX = 10 + offsetX;
  int calibButtonY = 90;
  int calibButtonW = 150;
  int calibButtonH = 30;

  uint16_t buttonColor = touchManager.isCalibrating() ? Colors::STATUS_YELLOW : Colors::BORDER_MAIN;
  tft.drawRoundRect(calibButtonX, calibButtonY, calibButtonW, calibButtonH, 5, buttonColor);

  tft.setTextColor(Colors::TEXT_MAIN);
  const char* buttonText = touchManager.isCalibrating() ? "Kalibrierung aktiv..." : "Kalibrierung starten";
  tft.drawString(buttonText, calibButtonX + 5, calibButtonY + 8, 1);

  // Kalibrierungs-Anweisungen
  if (touchManager.isCalibrating()) {
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("Kalibrierung aktiv:", 10 + offsetX, 130, 1);
    tft.drawString("Touchscreen wird neu", 10 + offsetX, 145, 1);
    tft.drawString("kalibriert...", 10 + offsetX, 160, 1);

    // Beenden-Button während Kalibrierung
    int stopButtonX = 170 + offsetX;
    int stopButtonY = 90;
    int stopButtonW = 100;
    int stopButtonH = 30;

    tft.drawRoundRect(stopButtonX, stopButtonY, stopButtonW, stopButtonH, 5, Colors::STATUS_RED);
    tft.setTextColor(Colors::TEXT_MAIN);
    tft.drawString("Beenden", stopButtonX + 25, stopButtonY + 8, 1);

  } else {
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("Touch-Genauigkeit optimieren", 10 + offsetX, 130, 1);
    tft.drawString("durch Neu-Kalibrierung", 10 + offsetX, 145, 1);
  }

  // System-Info
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("System-Information:", 10 + offsetX, 180, 1);

  tft.drawString("Firmware: ESP32 Rev2", 10 + offsetX, 195, 1);

  char uptimeInfo[32];
  snprintf(uptimeInfo, sizeof(uptimeInfo), "Uptime: %luh", appState.systemStatus.uptime / 3600);
  tft.drawString(uptimeInfo, 10 + offsetX, 210, 1);

  char memInfo[32];
  snprintf(memInfo, sizeof(memInfo), "RAM: %luKB frei", appState.systemStatus.freeHeap / 1024);
  tft.drawString(memInfo, 10 + offsetX, 225, 1);

  // Phase 0 Smoke-Test: Energie-Flow-Balken am unteren Rand
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("Energie-Flow Test:", 10, 160, 1);
  drawEnergyFlowZone(2, 175, 316, 55);

  Serial.println("Settings-Screen vollständig gezeichnet");
}

#if FEATURE_DEVICES_SCREEN
void drawDevicesScreen() {
  Serial.println("Zeichne Geraete-Screen...");

  tft.fillScreen(Colors::BG_MAIN);
  int offsetX = appState.antiBurnin.getOffsetX();
  int offsetY = appState.antiBurnin.getOffsetY();

  // Titel
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Geraete", 10 + offsetX, 10 + offsetY, 2);

  // Zurück-Button (oben rechts), gleiche Position wie auf anderen Detail-Screens
  int backButtonX = 270 + offsetX;
  int backButtonY = 10 + offsetY;
  tft.drawRoundRect(backButtonX, backButtonY, 40, 20, 3, Colors::BORDER_MAIN);
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Zurueck", backButtonX + 3, backButtonY + 6, 1);

  // Ein Toggle-Button je SmartPlug
  using namespace DevicesScreenLayout;
  for (int i = 0; i < SMART_PLUG_COUNT; i++) {
    const SmartPlugState& plug = appState.smartPlugs[i];
    int btnY = BTN_Y_START + i * BTN_SPACING + offsetY;
    int btnX = BTN_X + offsetX;

    uint16_t btnColor = plug.isOn ? 0x0320 : 0x2104;
    tft.fillRoundRect(btnX, btnY, BTN_W, BTN_H, Layout::SENSOR_BOX_RADIUS, btnColor);
    tft.drawRoundRect(btnX, btnY, BTN_W, BTN_H, Layout::SENSOR_BOX_RADIUS, Colors::BORDER_MAIN);

    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString(plug.name, btnX + Layout::PADDING_MEDIUM, btnY + Layout::PADDING_MEDIUM, 2);

    tft.setTextColor(Colors::TEXT_MAIN);
    const char* stateText = plug.isTimedOut ? "Offline" : (plug.isOn ? "EIN" : "AUS");
    tft.drawString(stateText, btnX + BTN_W - 90, btnY + Layout::PADDING_MEDIUM, 2);
  }

  Serial.println("Geraete-Screen vollständig gezeichnet");
}
#endif // FEATURE_DEVICES_SCREEN

// Zeichnet ein einzelnes Segment mit abgerundeten Außenkanten (nur an den
// Enden des Gesamtbalkens, nicht zwischen den Segmenten) und einem dezenten
// hellen Glanzstreifen oben für einen etwas plastischeren Look.
static void drawFlowSegment(int x, int y, int w, int h, uint16_t color,
                             bool roundLeft, bool roundRight) {
  if (w <= 0) return;
  const int r = min(4, h / 2);
  if (roundLeft && roundRight && w >= 2 * r) {
    tft.fillRoundRect(x, y, w, h, r, color);
  } else if (roundLeft && w >= r) {
    tft.fillRoundRect(x, y, r * 2, h, r, color);
    if (w > r) tft.fillRect(x + r, y, w - r, h, color);
  } else if (roundRight && w >= r) {
    tft.fillRoundRect(x + w - r * 2, y, r * 2, h, r, color);
    if (w > r) tft.fillRect(x, y, w - r, h, color);
  } else {
    tft.fillRect(x, y, w, h, color);
  }
  // Glanzstreifen: helle 1px-Linie im oberen Drittel
  tft.drawFastHLine(x + (roundLeft ? 1 : 0), y + 1, w - (roundLeft ? 1 : 0) - (roundRight ? 1 : 0),
                     lgfx::color565(255, 255, 255));
}

// Blendet den kW-Wert eines Segments zentriert ein, wenn genug Breite dafür da ist.
static void drawFlowSegmentLabel(int x, int w, int y, int h, float valueKW) {
  if (w < 30 || valueKW < 0.05f) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "%.1f", valueKW);
  tft.setTextColor(lgfx::color565(0, 0, 0));
  tft.setTextDatum(textdatum_t::middle_center);
  tft.drawString(buf, x + w / 2, y + h / 2, 1);
  tft.setTextDatum(textdatum_t::top_left);
}

void drawEnergyFlowZone(int x, int y, int w, int h) {
  // "Geldbeutel-Ampel": Eine prominente PV-Zahl (Wie viel produziere ich?) und
  // EIN Herkunfts-Balken (Woher kommt mein Verbrauch - PV, Speicher oder Netz?).
  // Nur 3 Ampelfarben: grün=PV (gut), blau=Hausspeicher (mittel), rot=Netz (schlecht).
  // Der Balken repräsentiert immer den vollen Gesamtverbrauch (loadPower, enthält
  // bereits die Wallbox); die Segmente zeigen dessen Herkunft. Keine Doppelzählung.
  const int BAR_H = 18;
  const int BAR_Y = y + 22;
  const int barW = w - 4;
  const float MAX_POWER = 15.0f; // Referenzgröße: bei 15kW ist der Balken voll

  // Hintergrund löschen
  tft.fillRect(x, y, w, h, Colors::BG_MAIN);

  float totalPV = max(0.0f, appState.pvPower);
  float totalConsumption = max(0.0f, appState.loadPower);

  // --- Herkunft des Gesamtverbrauchs ---
  float pvDirect = min(totalPV, totalConsumption);            // grün
  float battUse = 0.0f, gridUse = 0.0f;
  if (totalConsumption > pvDirect) {
    float remaining = totalConsumption - pvDirect;
    // Speicher liefert nur, wenn er (geschätzt) gerade nicht lädt.
    if (!appState.isStorageCharging && appState.storagePower > 0.01f) {
      battUse = min(appState.storagePower, remaining);        // blau
      gridUse = max(0.0f, remaining - battUse);               // rot
    } else {
      gridUse = remaining;                                    // rot
    }
  }

  // --- Zeile A: PV-Erzeugung groß, Verbrauch klein rechts ---
  char buf[24];
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("PV", x, y + 4, 1);
  // Ampel-Staffelung: grün ab 7 kWh, gelb ab 2 kWh, sonst rot (< 1 kWh knapp)
  uint16_t pvValueColor = (totalPV >= 7.0f) ? Colors::STATUS_GREEN
                         : (totalPV >= 2.0f) ? Colors::STATUS_YELLOW
                         : Colors::STATUS_RED;
  tft.setTextColor(pvValueColor);
  snprintf(buf, sizeof(buf), "%.1f kW", totalPV);
  tft.drawString(buf, x + 18, y + 2, &fonts::DejaVu12);

  tft.setTextColor(Colors::TEXT_LABEL);
  if (appState.isGridFeedIn && (totalPV - totalConsumption) > 0.05f) {
    // Einspeisung = überschüssiger PV wird verkauft (gut) → grün, nicht rot.
    // Rot ist nur der echte Netzbezug (im Herkunfts-Balken).
    snprintf(buf, sizeof(buf), "Einsp. %.1f kW", totalPV - totalConsumption);
    tft.setTextColor(Colors::STATUS_GREEN);
    tft.drawString(buf, x + barW - 96, y + 4, 1);
    tft.setTextColor(Colors::TEXT_LABEL);
  } else {
    snprintf(buf, sizeof(buf), "Verbrauch %.1f kW", totalConsumption);
    tft.drawString(buf, x + barW - 108, y + 4, 1);
  }

  // --- Zeile B: Herkunfts-Balken, Länge proportional zu MAX_POWER (15kW=voll) ---
  float fillRatio = min(totalConsumption / MAX_POWER, 1.0f);
  int filledW = max(0, (int)(barW * fillRatio));

  if (filledW > 0 && totalConsumption > 0.01f) {
    int pvW = (int)(filledW * pvDirect / totalConsumption);
    int batW = (int)(filledW * battUse / totalConsumption);
    int gridW = filledW - pvW - batW; // Rest gegen Rundungslücke

    int cx = x + 2;
    if (pvW > 0)   { drawFlowSegment(cx, BAR_Y, pvW,   BAR_H, Colors::STATUS_GREEN,        cx == x + 2, batW == 0 && gridW == 0); drawFlowSegmentLabel(cx, pvW,   BAR_Y, BAR_H, pvDirect); cx += pvW; }
    if (batW > 0)  { drawFlowSegment(cx, BAR_Y, batW,  BAR_H, Colors::STORAGE_HOUSE,       cx == x + 2, gridW == 0); drawFlowSegmentLabel(cx, batW,  BAR_Y, BAR_H, battUse); cx += batW; }
    if (gridW > 0) { drawFlowSegment(cx, BAR_Y, gridW, BAR_H, Colors::STATUS_RED,          cx == x + 2, true); drawFlowSegmentLabel(cx, gridW, BAR_Y, BAR_H, gridUse); }
  }
  tft.drawRoundRect(x + 2, BAR_Y, barW, BAR_H, min(4, BAR_H / 2), Colors::BORDER_PROGRESS);

  // --- Zeile C: Mini-Labels je Herkunft (nur was > 0 ist) ---
  int labelY = BAR_Y + BAR_H + 3;
  tft.setTextColor(Colors::TEXT_LABEL);
  char legend[40];
  int p = 0;
  if (pvDirect > 0.05f) p += snprintf(legend + p, sizeof(legend) - p, "PV %.1f  ", pvDirect);
  if (battUse  > 0.05f) p += snprintf(legend + p, sizeof(legend) - p, "Speicher %.1f  ", battUse);
  if (gridUse  > 0.05f) p += snprintf(legend + p, sizeof(legend) - p, "Netz %.1f", gridUse);
  if (p > 0) tft.drawString(legend, x + 2, labelY, 1);

  Serial.printf("EnergyFlowZone: PV=%.1f Load=%.1f | PVdirect=%.1f Batt=%.1f Grid=%.1f FeedIn=%d\n",
                totalPV, totalConsumption, pvDirect, battUse, gridUse, appState.isGridFeedIn);
}

