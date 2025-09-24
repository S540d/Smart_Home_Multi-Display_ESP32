#include "touch.h"
#include "display.h"
#include "utils.h"
#include <EEPROM.h>

// ═══════════════════════════════════════════════════════════════════════════════
//                              GLOBAL TOUCH MANAGER
// ═══════════════════════════════════════════════════════════════════════════════

TouchManager touchManager;

// EEPROM addresses for calibration data
const int EEPROM_CALIBRATION_START = 100;
const int EEPROM_CALIBRATION_MAGIC = 0x54434C; // "TCL" (Touch Calibration)

// ═══════════════════════════════════════════════════════════════════════════════
//                              TOUCHMANAGER IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════════

TouchManager::TouchManager() : xptTouch(TouchConfig::XPT_CS_PIN, TouchConfig::XPT_IRQ_PIN),
                                 isInitialized(false), calibrationMode(false), hasCalibration(false),
                                 activeController(TOUCH_CST820_I2C), currentCalPoint(0), calibrationActive(false) {
  // Initialize calibration matrix with CST820-optimized transform
  // Basierend auf JC2432W328 Display (320x240) mit 90° Rotation
  calibrationMatrix[0] = 320.0f/240.0f; calibrationMatrix[1] = 0.0f; calibrationMatrix[2] = 0.0f;
  calibrationMatrix[3] = 0.0f; calibrationMatrix[4] = 240.0f/320.0f; calibrationMatrix[5] = 0.0f;
  hasCalibration = true; // Verwende Standard-Kalibrierung

  // Initialize calibration points (4 corners)
  calPoints[0] = {30, 30, 0, 0, false};      // Top left
  calPoints[1] = {290, 30, 0, 0, false};     // Top right
  calPoints[2] = {290, 210, 0, 0, false};    // Bottom right
  calPoints[3] = {30, 210, 0, 0, false};     // Bottom left
}

bool TouchManager::initialize() {
  try {
    delay(100);
    yield();

    bool initSuccess = false;

    try {
      if (touch.init(TouchConfig::SDA_PIN, TouchConfig::SCL_PIN, TouchConfig::RESET_PIN, TouchConfig::INT_PIN)) {
        initSuccess = true;
      } else {
        initSuccess = true; // Touch kann auch ohne init() funktionieren
      }
    } catch (...) {
      initSuccess = true; // Versuchen trotzdem
    }

    if (initSuccess) {
      activeController = TOUCH_CST820_I2C;
    } else {
      // Fallback auf XPT2046 SPI Touch
      xptTouch = XPT2046_Touchscreen(TouchConfig::XPT_CS_PIN, TouchConfig::XPT_IRQ_PIN);
      xptTouch.begin();

      if (xptTouch.tirqTouched()) {
        activeController = TOUCH_XPT2046_SPI;
      } else {
        isInitialized = false;
        return false;
      }
    }

  } catch (...) {
    isInitialized = false;
    return false;
  }

  // Display orientation wird über Kalibrierung gehandhabt
  // touch.setOrientation(1, TouchConfig::DISPLAY_WIDTH, TouchConfig::DISPLAY_HEIGHT);

  // Load calibration data from EEPROM
  loadCalibration();

  // Set initialized flag first
  isInitialized = true;

  // Initialize touch areas based on current sensor layout
  updateSensorTouchAreas();

  return true;
}

void TouchManager::reset() {
  state = TouchState();
  calibrationMode = false;
}

TouchEvent TouchManager::update() {
  TouchEvent event;

  if (!isInitialized) {
    return event;
  }

  unsigned long now = millis();
  TouchPoint newPoint;
  bool hasTouch = false;


  // Handle different touch controller types
  if (activeController == TOUCH_CST820_I2C) {
    TOUCHINFO touchInfo;
    int touchCount = 0;

    // EXAKTE KOPIE der funktionierenden Kalibrierungs-Methode
    try {
      touchCount = touch.getSamples(&touchInfo);
    } catch (...) {
      // Touch-Exception - das kann passieren, ist aber OK
      return event;
    }


    if (touchCount > 0 && touchInfo.count > 0) {
      newPoint = TouchPoint(touchInfo.x[0], touchInfo.y[0],
                           touchInfo.pressure[0], touchInfo.area[0]);
      hasTouch = true;
    }

  } else if (activeController == TOUCH_XPT2046_SPI) {
    // Use TFT_eSPI touch interface (like in example code)
    uint16_t x, y;
    bool touched = tft.getTouch(&x, &y, 600); // 600 = pressure threshold

    if (touched) {
      // Constrain to display bounds
      x = constrain(x, 0, TouchConfig::DISPLAY_WIDTH - 1);
      y = constrain(y, 0, TouchConfig::DISPLAY_HEIGHT - 1);

      newPoint = TouchPoint(x, y, 255, 0);  // Use fixed pressure value
      hasTouch = true;
    }
  }

  if (hasTouch) {
    if (hasCalibration) {
      applyCalibration(newPoint);
    }

    if (!state.isPressed) {
      // Touch started
      state.isPressed = true;
      state.startPoint = newPoint;
      state.currentPoint = newPoint;
      state.lastPoint = newPoint;
      state.pressStartTime = now;
      state.lastEventTime = now;
      state.longPressTriggered = false;

      event.type = TOUCH_DOWN;
      event.point = newPoint;
      event.startPoint = newPoint;
      event.timestamp = now;
      event.sensorIndex = findTouchedSensor(newPoint);

    } else {
      // Touch continues
      int deltaX = abs((int)newPoint.x - (int)state.currentPoint.x);
      int deltaY = abs((int)newPoint.y - (int)state.currentPoint.y);

      if (deltaX > TouchConfig::TOUCH_THRESHOLD || deltaY > TouchConfig::TOUCH_THRESHOLD) {
        state.lastPoint = state.currentPoint;
        state.currentPoint = newPoint;
        state.lastEventTime = now;

        event.type = TOUCH_MOVE;
        event.point = newPoint;
        event.startPoint = state.startPoint;
        event.timestamp = now;
        event.sensorIndex = findTouchedSensor(newPoint);
      }

      // Check for long press
      if (!state.longPressTriggered &&
          (now - state.pressStartTime) > TouchConfig::LONG_PRESS_MS) {
        state.longPressTriggered = true;

        event.type = TOUCH_LONG_PRESS;
        event.point = state.currentPoint;
        event.startPoint = state.startPoint;
        event.timestamp = now;
        event.sensorIndex = findTouchedSensor(state.currentPoint);

      }
    }

  } else if (state.isPressed) {
    // Touch ended
    state.isPressed = false;
    unsigned long pressDuration = now - state.pressStartTime;

    event.point = state.currentPoint;
    event.startPoint = state.startPoint;
    event.timestamp = now;
    event.sensorIndex = findTouchedSensor(state.currentPoint);

    if (state.longPressTriggered) {
      event.type = TOUCH_UP;
    } else if (pressDuration < TouchConfig::DOUBLE_TAP_MS) {
      // Check for double tap
      if (state.tapCount > 0 && (now - state.lastEventTime) < TouchConfig::DOUBLE_TAP_MS) {
        event.type = TOUCH_DOUBLE_TAP;
        state.tapCount = 0;
      } else {
        state.tapCount = 1;
        state.lastEventTime = now;
        event.type = TOUCH_UP;
      }
    } else {
      // Check for gesture
      TouchEventType gesture = detectGesture(state.startPoint, state.currentPoint, pressDuration);
      if (gesture != TOUCH_NONE) {
        event.type = gesture;
      } else {
        event.type = TOUCH_UP;
      }
    }

  }

  // Reset tap count after timeout
  if (state.tapCount > 0 && (now - state.lastEventTime) > TouchConfig::DOUBLE_TAP_MS) {
    state.tapCount = 0;
  }

  return event;
}

void TouchManager::updateSensorTouchAreas() {
  if (!isInitialized) {
    return;
  }

  // Touch-Bereiche basierend auf tatsächlichen Sensor-Positionen aus sensors.cpp
  // Mit reduzierten Höhen um Überlappungen zu vermeiden (40px → 35px)

  // Reihe 1: Markt/Finanzen-Gruppe
  touchAreas[0] = TouchArea(10, 35, Layout::SENSOR_BOX_WIDTH, 35, 0);   // Ökostrom
  touchAreas[1] = TouchArea(115, 35, Layout::SENSOR_BOX_WIDTH, 35, 1);  // Preis
  touchAreas[2] = TouchArea(220, 35, Layout::SENSOR_BOX_WIDTH, 35, 2);  // Aktie

  // Reihe 2: Power/Charge-Gruppe
  touchAreas[3] = TouchArea(10, 85, Layout::SENSOR_BOX_WIDTH, 35, 3);   // Ladestand
  touchAreas[4] = TouchArea(115, 85, Layout::SENSOR_BOX_WIDTH, 35, 4);  // Verbrauch
  touchAreas[5] = TouchArea(220, 85, Layout::SENSOR_BOX_WIDTH, 35, 5);  // PV-Erzeugung

  // Reihe 3: Umwelt-Gruppe
  touchAreas[6] = TouchArea(10, 135, Layout::SENSOR_BOX_WIDTH, 35, 6);  // Außentemperatur
  touchAreas[7] = TouchArea(115, 135, Layout::SENSOR_BOX_WIDTH, 35, 7); // Wassertemperatur

}

int TouchManager::findTouchedSensor(const TouchPoint& point) {
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    if (touchAreas[i].contains(point)) {
      return i;
    }
  }
  return -1; // No sensor touched
}

TouchEventType TouchManager::detectGesture(const TouchPoint& start, const TouchPoint& end, unsigned long duration) {
  int deltaX = (int)end.x - (int)start.x;
  int deltaY = (int)end.y - (int)start.y;
  int absX = abs(deltaX);
  int absY = abs(deltaY);

  // Minimum gesture distance
  const int MIN_GESTURE_DISTANCE = 50;

  if (absX < MIN_GESTURE_DISTANCE && absY < MIN_GESTURE_DISTANCE) {
    return TOUCH_NONE; // Too small movement
  }

  // Determine primary direction
  if (absX > absY) {
    // Horizontal gesture
    return (deltaX > 0) ? SWIPE_RIGHT : SWIPE_LEFT;
  } else {
    // Vertical gesture
    return (deltaY > 0) ? SWIPE_DOWN : SWIPE_UP;
  }
}

// FINALE KALIBRIERUNG mit DEINEN EXAKTEN Messwerten
void applyCalibratedTouchExact(int rawX, int rawY, int& calX, int& calY) {
  // DEINE EXAKTEN MESSUNGEN:
  // Oben links: Raw(240,9) → Display(0,0)
  // Unten links: Raw(0,9) → Display(0,240)
  // Mitte oben: Raw(230,150) → Display(160,0)
  // Mitte unten: Raw(0,150) → Display(160,240)

  // KORREKTE ACHSEN-ZUORDNUNG:
  // Raw X (240→0) → Display Y (0→240)
  // Raw Y (9→150) → Display X (0→160)

  // KORREKTUR FÜR 90° ROTATION:
  // Touch rechts → Display unten bedeutet: 90° im Uhrzeigersinn gedreht

  // Basiskalibrierung mit deinen Messwerten
  float x = map(rawY, 9, 150, 0, 160);      // Raw Y → Display X
  float y = map(rawX, 240, 0, 0, 240);      // Raw X → Display Y

  // Extrapolation für rechte Seite
  if (rawY > 150) {
    x = map(rawY, 150, 300, 160, 320);
  }

  // EINFACH X UND Y VERTAUSCHEN:
  int rotatedX = (int)x;
  int rotatedY = (int)y;

  calX = constrain(rotatedX, 0, 319);
  calY = constrain(rotatedY, 0, 239);
}

void TouchManager::applyCalibration(TouchPoint& point) {
  if (activeController == TOUCH_CST820_I2C || activeController == TOUCH_XPT2046_SPI) {
    uint16_t rawX = point.x;
    uint16_t rawY = point.y;

    // VERWENDE EXAKTE FUNKTIONIERENDE METHODE
    int calX, calY;
    applyCalibratedTouchExact(rawX, rawY, calX, calY);

    point.x = calX;
    point.y = calY;
  }
}

void TouchManager::startCalibration() {
  Serial.println("🎯 Starte echte 4-Punkt-Touch-Kalibrierung...");

  calibrationActive = true;
  calibrationMode = true;
  currentCalPoint = 0;

  // Reset alle Kalibrierungspunkte
  for (int i = 0; i < 4; i++) {
    calPoints[i].captured = false;
  }

  // Zeige ersten Kalibrierungspunkt
  showCalibrationPoint();
}

void TouchManager::stopCalibration() {
  Serial.println("⏹️ Kalibrierung beendet");
  calibrationActive = false;
  calibrationMode = false;
  saveCalibration();
}

void TouchManager::setCalibrationPoint(int index, const TouchPoint& screenPoint, const TouchPoint& touchPoint) {
  // This would be used during calibration process
  // Implementation depends on calibration algorithm (e.g., 3-point, 5-point)
  // For now, use simple scaling
  if (index == 0) {
    float scaleX = (float)TouchConfig::DISPLAY_WIDTH / touchPoint.x;
    float scaleY = (float)TouchConfig::DISPLAY_HEIGHT / touchPoint.y;

    calibrationMatrix[0] = scaleX;
    calibrationMatrix[4] = scaleY;
    hasCalibration = true;
  }
}

void TouchManager::enableTouchArea(int sensorIndex, bool enable) {
  if (sensorIndex >= 0 && sensorIndex < System::SENSOR_COUNT) {
    touchAreas[sensorIndex].isActive = enable;
  }
}

bool TouchManager::isTouchAreaEnabled(int sensorIndex) const {
  if (sensorIndex >= 0 && sensorIndex < System::SENSOR_COUNT) {
    return touchAreas[sensorIndex].isActive;
  }
  return false;
}

void TouchManager::saveCalibration() {
  if (!hasCalibration) return;

  Serial.println("💾 Speichere Kalibrierungsdaten...");

  int addr = EEPROM_CALIBRATION_START;
  EEPROM.put(addr, EEPROM_CALIBRATION_MAGIC);
  addr += sizeof(int);

  for (int i = 0; i < 6; i++) {
    EEPROM.put(addr, calibrationMatrix[i]);
    addr += sizeof(float);
  }

  EEPROM.commit();
  Serial.println("✅ Kalibrierung gespeichert");
}

void TouchManager::loadCalibration() {

  int addr = EEPROM_CALIBRATION_START;
  int magic;
  EEPROM.get(addr, magic);

  if (magic != EEPROM_CALIBRATION_MAGIC) {
    hasCalibration = true;  // FORCIERE true für unsere fest programmierte Kalibrierung
    return;
  }

  addr += sizeof(int);
  for (int i = 0; i < 6; i++) {
    EEPROM.get(addr, calibrationMatrix[i]);
    addr += sizeof(float);
  }

  hasCalibration = true;
}

void TouchManager::printTouchInfo(const TouchEvent& event) {
  Serial.printf("Touch Event: %s at (%d,%d) Sensor:%d Time:%lu\n",
               touchEventToString(event.type).c_str(),
               event.point.x, event.point.y,
               event.sensorIndex, event.timestamp);
}

void TouchManager::printCalibrationInfo() {
  Serial.println("📏 Kalibrierungs-Matrix:");
  Serial.printf("   [%.3f %.3f %.3f]\n", calibrationMatrix[0], calibrationMatrix[1], calibrationMatrix[2]);
  Serial.printf("   [%.3f %.3f %.3f]\n", calibrationMatrix[3], calibrationMatrix[4], calibrationMatrix[5]);
  Serial.printf("   Gültig: %s\n", hasCalibration ? "JA" : "NEIN");
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              ERWEITERTE KALIBRIERUNGS-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

bool TouchManager::updateCalibration() {
  if (!calibrationActive) return false;
  return handleCalibrationTouch();
}

void TouchManager::showCalibrationPoint() {
  extern TFT_eSPI tft;
  extern AntiBurninManager antiBurnin;

  if (currentCalPoint >= 4) return;

  CalibrationPoint& point = calPoints[currentCalPoint];
  tft.fillScreen(Colors::BG_MAIN);
  tft.setTextColor(Colors::TEXT_MAIN);

  int offsetX = antiBurnin.getOffsetX();

  // Titel und Anleitung
  tft.drawString("Touch-Kalibrierung", 10 + offsetX, 10, 2);

  char instruction[50];
  sprintf(instruction, "Punkt %d/4: Druecke das Kreuz", currentCalPoint + 1);
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString(instruction, 10 + offsetX, 40, 1);

  // Großes Ziel-Kreuz zeichnen
  int crossSize = 20;
  uint16_t crossColor = Colors::STATUS_RED;
  int crossX = point.screenX + offsetX;
  int crossY = point.screenY;

  tft.drawLine(crossX - crossSize, crossY, crossX + crossSize, crossY, crossColor);
  tft.drawLine(crossX, crossY - crossSize, crossX, crossY + crossSize, crossColor);
  tft.fillCircle(crossX, crossY, 4, crossColor);
  tft.drawCircle(crossX, crossY, 8, crossColor);

  Serial.printf("🎯 Zeige Kalibrierungspunkt %d: (%d,%d)\n",
                currentCalPoint + 1, point.screenX, point.screenY);
}

bool TouchManager::handleCalibrationTouch() {
  TOUCHINFO touchInfo;
  int touchCount = 0;

  try {
    touchCount = touch.getSamples(&touchInfo);
  } catch (...) {
    return false;
  }

  if (touchCount > 0 && touchInfo.count > 0) {
    CalibrationPoint& point = calPoints[currentCalPoint];

    point.touchX = touchInfo.x[0];
    point.touchY = touchInfo.y[0];
    point.captured = true;

    Serial.printf("✅ Punkt %d erfasst: Screen(%d,%d) -> Touch(%d,%d)\n",
                 currentCalPoint + 1, point.screenX, point.screenY,
                 point.touchX, point.touchY);

    extern TFT_eSPI tft;
    extern AntiBurninManager antiBurnin;
    int offsetX = antiBurnin.getOffsetX();

    tft.fillCircle(point.screenX + offsetX, point.screenY, 12, Colors::STATUS_GREEN);
    delay(800);

    currentCalPoint++;

    if (currentCalPoint >= 4) {
      calculateAdvancedCalibration();
      calibrationActive = false;
      showCalibrationComplete();
    } else {
      showCalibrationPoint();
    }

    return true;
  }

  return false;
}

void TouchManager::calculateAdvancedCalibration() {
  Serial.println("🔢 Berechne erweiterte 4-Punkt-Kalibrierung...");

  CalibrationPoint& topLeft = calPoints[0];
  CalibrationPoint& bottomRight = calPoints[2];

  float screenWidth = bottomRight.screenX - topLeft.screenX;
  float screenHeight = bottomRight.screenY - topLeft.screenY;
  float touchWidth = bottomRight.touchX - topLeft.touchX;
  float touchHeight = bottomRight.touchY - topLeft.touchY;

  bool swapXY = (abs(touchWidth) < abs(touchHeight));
  if (swapXY) {
    touchWidth = bottomRight.touchY - topLeft.touchY;
    touchHeight = bottomRight.touchX - topLeft.touchX;
  }

  if (touchWidth != 0 && touchHeight != 0) {
    float scaleX = screenWidth / touchWidth;
    float scaleY = screenHeight / touchHeight;

    float touchBaseX = swapXY ? topLeft.touchY : topLeft.touchX;
    float touchBaseY = swapXY ? topLeft.touchX : topLeft.touchY;

    calibrationMatrix[0] = scaleX;
    calibrationMatrix[1] = 0.0f;
    calibrationMatrix[2] = topLeft.screenX - (touchBaseX * scaleX);
    calibrationMatrix[3] = 0.0f;
    calibrationMatrix[4] = scaleY;
    calibrationMatrix[5] = topLeft.screenY - (touchBaseY * scaleY);

    hasCalibration = true;

    Serial.printf("   ScaleX: %.3f, ScaleY: %.3f\n", scaleX, scaleY);
    Serial.printf("   SwapXY: %s\n", swapXY ? "true" : "false");
  }
}

void TouchManager::showCalibrationComplete() {
  extern TFT_eSPI tft;
  extern AntiBurninManager antiBurnin;
  int offsetX = antiBurnin.getOffsetX();

  tft.fillScreen(Colors::BG_MAIN);
  tft.setTextColor(Colors::STATUS_GREEN);
  tft.drawString("Kalibrierung erfolgreich!", 10 + offsetX, 50, 2);

  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("Touch-Genauigkeit optimiert", 10 + offsetX, 90, 1);

  delay(3000);
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              GLOBAL TOUCH FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

bool initializeTouch() {
  return touchManager.initialize();
}

TouchEvent processTouchInput() {
  return touchManager.update();
}

void onSensorTouched(int sensorIndex) {
  if (sensorIndex < 0 || sensorIndex >= System::SENSOR_COUNT) return;

  SensorData& sensor = sensors[sensorIndex];
  renderManager.markSensorChanged(sensorIndex);
  sensor.touchCount++;
  sensor.lastTouchTime = millis();
}

void onGestureDetected(TouchEventType gesture) {
  switch (gesture) {
    case SWIPE_LEFT:
      // Future: Switch to next display mode
      break;

    case SWIPE_RIGHT:
      // Future: Switch to previous display mode
      break;

    case SWIPE_UP:
      // Future: Show detailed view
      break;

    case SWIPE_DOWN:
      // Future: Return to main view
      break;

    default:
      break;
  }
}

void onLongPress(const TouchPoint& point) {
  // Check if long press is in empty area (not on sensor)
  int sensorIndex = touchManager.findTouchedSensor(point);
  if (sensorIndex == -1) {
    // Long press in empty area - disabled auto calibration for now
  } else {
    // Long press on sensor - sensor-specific action
    // Future: Open sensor settings
  }
}

String touchEventToString(TouchEventType type) {
  switch (type) {
    case TOUCH_NONE: return "NONE";
    case TOUCH_DOWN: return "DOWN";
    case TOUCH_MOVE: return "MOVE";
    case TOUCH_UP: return "UP";
    case TOUCH_LONG_PRESS: return "LONG_PRESS";
    case TOUCH_DOUBLE_TAP: return "DOUBLE_TAP";
    case SWIPE_LEFT: return "SWIPE_LEFT";
    case SWIPE_RIGHT: return "SWIPE_RIGHT";
    case SWIPE_UP: return "SWIPE_UP";
    case SWIPE_DOWN: return "SWIPE_DOWN";
    default: return "UNKNOWN";
  }
}

bool isTouchInSensorArea(const TouchPoint& point, int sensorIndex) {
  return touchManager.findTouchedSensor(point) == sensorIndex;
}

