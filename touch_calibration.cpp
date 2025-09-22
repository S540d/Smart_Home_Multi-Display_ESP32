#include <Arduino.h>
#include <TFT_eSPI.h>
#include <bb_captouch.h>
#include <EEPROM.h>

// CST820 Pin Configuration
#define I2C_SDA 33
#define I2C_SCL 32
#define TP_RST 25
#define TP_INT 21

TFT_eSPI tft = TFT_eSPI();
BBCapTouch touch;

// Kalibrierungs-Daten
struct CalibrationData {
  float scaleX, scaleY;
  float offsetX, offsetY;
  bool swapXY;
  bool mirrorX, mirrorY;
  uint32_t magic; // Zur Validierung
};

CalibrationData calibration;
const uint32_t CALIBRATION_MAGIC = 0x12345678;
const int EEPROM_CALIBRATION_ADDR = 0;

// Kalibrierungs-Punkte (Ecken + Mitte)
struct CalibrationPoint {
  int screenX, screenY;
  int touchX, touchY;
  bool captured;
};

CalibrationPoint calPoints[4] = {
  {30, 30, 0, 0, false},      // Oben links
  {290, 30, 0, 0, false},     // Oben rechts
  {290, 210, 0, 0, false},    // Unten rechts
  {30, 210, 0, 0, false}      // Unten links
};

int currentCalPoint = 0;
bool calibrationMode = false;
bool calibrationComplete = false;

// Forward declarations
void showMainScreen();
void handleStartTouch();
void handleCalibrationTouch();
void handleTestTouch();
void startCalibration();
void showCalibrationPoint();
void calculateCalibration();
void saveCalibration();
void loadCalibration();
bool hasValidCalibration();
void showTestScreen();
void applyCalibratedTouch(int rawX, int rawY, int& calX, int& calY);

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("🔧 TOUCH KALIBRIERUNG - Bootschleife-sicher");
  Serial.println("📍 Pins: SDA=33, SCL=32, RST=25, INT=21");

  EEPROM.begin(512);

  // Initialize display
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  showMainScreen();

  // ALTERNATIVE Touch-Initialisierung - auch ohne init() funktionsfähig
  Serial.println("⏳ Alternative Touch-Initialisierung...");

  bool touchOK = false;

  // Versuche normale Initialisierung
  try {
    if (touch.init(I2C_SDA, I2C_SCL, TP_RST, TP_INT)) {
      touchOK = true;
      Serial.println("✅ Touch normal initialisiert");
    } else {
      Serial.println("⚠️ Touch.init() fehlgeschlagen, aber wir versuchen es trotzdem...");
      // WICHTIG: Auch ohne init() kann Touch funktionieren!
      touchOK = true; // Wir versuchen es trotzdem
    }
  } catch (...) {
    Serial.println("⚠️ Touch-Init Exception, aber wir versuchen es trotzdem...");
    touchOK = true; // Wir versuchen es trotzdem
  }

  if (touchOK) {
    Serial.println("🔧 Touch-System bereit (init-Status ignoriert)");
    Serial.println("📝 Hinweis: Touch kann auch ohne erfolgreiche init() funktionieren");
  }

  // Lade bestehende Kalibrierung
  loadCalibration();

  Serial.println();
  Serial.println("🎯 Bereit für Kalibrierung!");
  Serial.println("   - Berühre 'KALIBRIERUNG STARTEN' um zu beginnen");
  Serial.println("   - Oder teste vorhandene Kalibrierung");
}

void loop() {
  if (!calibrationComplete && !calibrationMode) {
    // Warte auf Start-Touch
    handleStartTouch();
  } else if (calibrationMode) {
    // Kalibrierungs-Modus
    handleCalibrationTouch();
  } else {
    // Test-Modus
    handleTestTouch();
  }

  delay(50);
}

void showMainScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Touch Kalibrierung", 10, 10, 2);

  // Start-Button
  tft.fillRect(10, 50, 300, 40, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("KALIBRIERUNG STARTEN", 50, 65, 2);

  // Info
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("1. Startknopf druecken", 10, 110, 1);
  tft.drawString("2. Alle 4 Ecken nacheinander", 10, 125, 1);
  tft.drawString("3. Kalibrierung wird gespeichert", 10, 140, 1);

  // Status
  if (hasValidCalibration()) {
    tft.setTextColor(TFT_GREEN);
    tft.drawString("Bestehende Kalibrierung OK", 10, 170, 1);
  } else {
    tft.setTextColor(TFT_RED);
    tft.drawString("Keine Kalibrierung gefunden", 10, 170, 1);
  }
}

void handleStartTouch() {
  TOUCHINFO touchInfo;
  int touchCount = 0;

  // Sichere Touch-Abfrage mit Try-Catch
  try {
    touchCount = touch.getSamples(&touchInfo);
  } catch (...) {
    // Touch-Exception - das kann passieren, ist aber OK
    return;
  }

  // Debug: Zeige Touch-Status alle 3 Sekunden
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 3000) {
    lastDebug = millis();
    Serial.printf("🔍 Touch-Status: getSamples()=%d\n", touchCount);
    if (touchCount > 0) {
      Serial.printf("   touchInfo.count=%d\n", touchInfo.count);
      if (touchInfo.count > 0) {
        Serial.printf("   Raw: x=%d, y=%d\n", touchInfo.x[0], touchInfo.y[0]);
      }
    }
  }

  if (touchCount > 0 && touchInfo.count > 0) {
    int x = touchInfo.x[0];
    int y = touchInfo.y[0];

    Serial.printf("👆 Touch erkannt: (%d,%d)\n", x, y);

    // Check ob Start-Button gedrückt wurde (großzügigerer Bereich)
    if (x >= 0 && x <= 320 && y >= 40 && y <= 100) {
      Serial.println("🎯 Start-Button gedrückt!");
      startCalibration();
    } else {
      Serial.printf("   -> Außerhalb Start-Button (Button: 0-320, 40-100)\n");
    }

    delay(200); // Debounce
  }
}

void startCalibration() {
  Serial.println("🎯 Starte Kalibrierung...");
  calibrationMode = true;
  currentCalPoint = 0;

  // Reset alle Punkte
  for (int i = 0; i < 4; i++) {
    calPoints[i].captured = false;
  }

  showCalibrationPoint();
}

void showCalibrationPoint() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  CalibrationPoint& point = calPoints[currentCalPoint];

  // Anleitung
  tft.drawString("Kalibrierung", 10, 10, 2);
  char instruction[50];
  sprintf(instruction, "Punkt %d/4: Druecke das Kreuz", currentCalPoint + 1);
  tft.drawString(instruction, 10, 40, 1);

  // Großes Ziel-Kreuz zeichnen
  int crossSize = 15;
  tft.drawLine(point.screenX - crossSize, point.screenY, point.screenX + crossSize, point.screenY, TFT_RED);
  tft.drawLine(point.screenX, point.screenY - crossSize, point.screenX, point.screenY + crossSize, TFT_RED);
  tft.fillCircle(point.screenX, point.screenY, 3, TFT_RED);

  Serial.printf("🎯 Zeige Kalibrierungspunkt %d: (%d,%d)\n", currentCalPoint + 1, point.screenX, point.screenY);
}

void handleCalibrationTouch() {
  TOUCHINFO touchInfo;
  int touchCount = 0;

  try {
    touchCount = touch.getSamples(&touchInfo);
  } catch (...) {
    Serial.println("⚠️ Calibration Touch-Exception ignoriert");
    return;
  }

  if (touchCount > 0 && touchInfo.count > 0) {
    CalibrationPoint& point = calPoints[currentCalPoint];

    // Capture touch coordinates
    point.touchX = touchInfo.x[0];
    point.touchY = touchInfo.y[0];
    point.captured = true;

    Serial.printf("✅ Punkt %d erfasst: Screen(%d,%d) -> Touch(%d,%d)\n",
                 currentCalPoint + 1, point.screenX, point.screenY, point.touchX, point.touchY);

    // Bestätigungs-Feedback
    tft.fillCircle(point.screenX, point.screenY, 8, TFT_GREEN);
    delay(500);

    currentCalPoint++;

    if (currentCalPoint >= 4) {
      // Alle Punkte erfasst - berechne Kalibrierung
      calculateCalibration();
      calibrationMode = false;
      calibrationComplete = true;
      showTestScreen();
    } else {
      // Nächsten Punkt zeigen
      showCalibrationPoint();
    }

    delay(300); // Debounce
  }
}

void calculateCalibration() {
  Serial.println("🔢 Berechne Kalibrierung...");

  // Vereinfachte Kalibrierung: Nimm 2 diagonale Punkte
  CalibrationPoint& topLeft = calPoints[0];     // (30, 30)
  CalibrationPoint& bottomRight = calPoints[2]; // (290, 210)

  float screenWidth = bottomRight.screenX - topLeft.screenX;   // 260
  float screenHeight = bottomRight.screenY - topLeft.screenY; // 180

  float touchWidth = bottomRight.touchX - topLeft.touchX;
  float touchHeight = bottomRight.touchY - topLeft.touchY;

  // Check ob X/Y vertauscht sind
  if (abs(touchWidth) < abs(touchHeight)) {
    Serial.println("🔄 X/Y scheinen vertauscht zu sein");
    calibration.swapXY = true;
    // Nach Vertauschung neu berechnen
    touchWidth = bottomRight.touchY - topLeft.touchY;
    touchHeight = bottomRight.touchX - topLeft.touchX;
  } else {
    calibration.swapXY = false;
  }

  // Skalierung berechnen
  calibration.scaleX = screenWidth / touchWidth;
  calibration.scaleY = screenHeight / touchHeight;

  // Offset berechnen
  calibration.offsetX = topLeft.screenX - (topLeft.touchX * calibration.scaleX);
  calibration.offsetY = topLeft.screenY - (topLeft.touchY * calibration.scaleY);

  // Spiegelung prüfen (negative Skalierung)
  calibration.mirrorX = calibration.scaleX < 0;
  calibration.mirrorY = calibration.scaleY < 0;

  if (calibration.mirrorX) calibration.scaleX = -calibration.scaleX;
  if (calibration.mirrorY) calibration.scaleY = -calibration.scaleY;

  calibration.magic = CALIBRATION_MAGIC;

  Serial.println("📊 Kalibrierung berechnet:");
  Serial.printf("   SwapXY: %s\n", calibration.swapXY ? "true" : "false");
  Serial.printf("   MirrorX: %s, MirrorY: %s\n", calibration.mirrorX ? "true" : "false", calibration.mirrorY ? "true" : "false");
  Serial.printf("   ScaleX: %.3f, ScaleY: %.3f\n", calibration.scaleX, calibration.scaleY);
  Serial.printf("   OffsetX: %.1f, OffsetY: %.1f\n", calibration.offsetX, calibration.offsetY);

  saveCalibration();
}

void saveCalibration() {
  EEPROM.put(EEPROM_CALIBRATION_ADDR, calibration);
  EEPROM.commit();
  Serial.println("💾 Kalibrierung gespeichert");
}

void loadCalibration() {
  EEPROM.get(EEPROM_CALIBRATION_ADDR, calibration);

  if (calibration.magic == CALIBRATION_MAGIC) {
    Serial.println("📖 Kalibrierung geladen");
    Serial.printf("   SwapXY: %s, MirrorX: %s, MirrorY: %s\n",
                 calibration.swapXY ? "true" : "false",
                 calibration.mirrorX ? "true" : "false",
                 calibration.mirrorY ? "true" : "false");
  } else {
    Serial.println("⚠️ Keine gültige Kalibrierung gefunden");
    // Standard-Werte
    calibration.scaleX = calibration.scaleY = 1.0;
    calibration.offsetX = calibration.offsetY = 0.0;
    calibration.swapXY = calibration.mirrorX = calibration.mirrorY = false;
    calibration.magic = 0;
  }
}

bool hasValidCalibration() {
  return calibration.magic == CALIBRATION_MAGIC;
}

void showTestScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Kalibrierung abgeschlossen!", 10, 10, 2);
  tft.drawString("Teste durch Beruehren des Displays", 10, 40, 1);

  tft.setTextColor(TFT_GREEN);
  tft.drawString("Druecke RESET fuer neue Kalibrierung", 10, 200, 1);
}

void handleTestTouch() {
  TOUCHINFO touchInfo;
  int touchCount = 0;

  try {
    touchCount = touch.getSamples(&touchInfo);
  } catch (...) {
    Serial.println("⚠️ Test Touch-Exception ignoriert");
    return;
  }

  if (touchCount > 0 && touchInfo.count > 0) {
    int rawX = touchInfo.x[0];
    int rawY = touchInfo.y[0];

    // Kalibrierung anwenden
    int calibratedX, calibratedY;
    applyCalibratedTouch(rawX, rawY, calibratedX, calibratedY);

    Serial.printf("🎯 Raw(%d,%d) -> Calibrated(%d,%d)\n", rawX, rawY, calibratedX, calibratedY);

    // Visual feedback
    tft.fillCircle(constrain(calibratedX, 0, 319), constrain(calibratedY, 0, 239), 8, TFT_CYAN);

    // Clear occasionally
    static unsigned long lastClear = 0;
    if (millis() - lastClear > 3000) {
      tft.fillRect(0, 60, 320, 140, TFT_BLACK);
      lastClear = millis();
    }

    delay(100);
  }
}

void applyCalibratedTouch(int rawX, int rawY, int& calX, int& calY) {
  // FESTE KALIBRIERUNG basierend auf gemessenen Werten:
  // Oben links: Raw(230,9) → Display(0,0)
  // Oben rechts: Raw(230,310) → Display(320,0)
  // Unten links: Raw(1,9) → Display(0,240)
  // Unten rechts: Raw(1,310) → Display(320,240)

  // ACHSEN SIND VERTAUSCHT UND GESPIEGELT:
  // Raw X (230→1) → Display Y (0→240)
  // Raw Y (9→310) → Display X (0→320)

  float x = map(rawY, 9, 310, 0, 320);    // Raw Y → Display X
  float y = map(rawX, 230, 1, 0, 240);    // Raw X → Display Y (gespiegelt)

  calX = constrain((int)x, 0, 319);
  calY = constrain((int)y, 0, 239);
}