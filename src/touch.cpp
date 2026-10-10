#include "touch.h"
#include "display.h"
#include "utils.h"
#include <EEPROM.h>
static SystemState& appState = SystemState::getInstance();


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

TouchManager::TouchManager() : isInitialized(false), calibrationMode(false), hasCalibration(false),
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

    // CST820 I2C Touch initialisieren (einziger Controller auf dem JC2432W328)
    try {
      if (!touch.init(TouchConfig::SDA_PIN, TouchConfig::SCL_PIN, TouchConfig::RESET_PIN, TouchConfig::INT_PIN)) {
        Serial.println("⚠️ CST820 init() fehlgeschlagen - Touch kann trotzdem funktionieren");
      }
    } catch (...) {
      Serial.println("⚠️ CST820 init() Exception - Touch kann trotzdem funktionieren");
    }
    activeController = TOUCH_CST820_I2C;

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


  // CST820 I2C Touch abfragen
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
  }

  if (hasTouch) {
    if (hasCalibration) {
      applyCalibration(newPoint);
    }

    if (!state.isPressed) {
      // Debounce: ignore touches within DEBOUNCE_MS of last touch-up
      if ((now - state.lastDebounceTime) < TouchConfig::DEBOUNCE_MS) {
        return event;
      }

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
    state.lastDebounceTime = now;
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

  int offsetX = appState.antiBurnin.getOffsetX();
  int offsetY = appState.antiBurnin.getOffsetY();

  // Touch-Areas direkt aus sensor.layout lesen – keine doppelte Koordinatenpflege
  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    const auto& layout = appState.sensors[i].layout;
    touchAreas[i] = TouchArea(
      layout.x + offsetX,
      layout.y + offsetY,
      layout.w,
      layout.h,
      i
    );
  }
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
  // Verbesserte Kalibrierung für gesamtes Display (320x240)
  // Basierend auf 90° Rotation: Raw X→Display Y, Raw Y→Display X

  // Touch-Bereiche erweitert für vollständige Abdeckung
  // Raw X: ~240→0 entspricht Display Y: 0→240
  // Raw Y: ~9→300 entspricht Display X: 0→320

  // Lineare Transformation mit erweiterten Bereichen
  float x = constrain(map(rawY, 5, 310, 0, 320), 0, 319);    // Raw Y → Display X (erweitert)
  float y = constrain(map(rawX, 250, -10, 0, 240), 0, 239);  // Raw X → Display Y (erweitert)

  // Kleine Korrektur für bessere Zentrierung
  x += 2;  // Leichter Offset nach rechts
  y -= 1;  // Leichter Offset nach oben

  // EINFACH X UND Y VERTAUSCHEN:
  int rotatedX = (int)x;
  int rotatedY = (int)y;

  calX = constrain(rotatedX, 0, 319);
  calY = constrain(rotatedY, 0, 239);
}

void TouchManager::applyCalibration(TouchPoint& point) {
  if (activeController == TOUCH_CST820_I2C) {
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

  if (currentCalPoint >= 4) return;

  CalibrationPoint& point = calPoints[currentCalPoint];
  tft.fillScreen(Colors::BG_MAIN);
  tft.setTextColor(Colors::TEXT_MAIN);

  int offsetX = appState.antiBurnin.getOffsetX();

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

    int offsetX = appState.antiBurnin.getOffsetX();

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
  int offsetX = appState.antiBurnin.getOffsetX();

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

  SensorData& sensor = appState.sensors[sensorIndex];
  appState.renderManager.markSensorChanged(sensorIndex);
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

