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
                                 activeController(TOUCH_CST820_I2C) {
  // Initialize calibration matrix with CST820-optimized transform
  // Basierend auf JC2432W328 Display (320x240) mit 90° Rotation
  calibrationMatrix[0] = 320.0f/240.0f; calibrationMatrix[1] = 0.0f; calibrationMatrix[2] = 0.0f;
  calibrationMatrix[3] = 0.0f; calibrationMatrix[4] = 240.0f/320.0f; calibrationMatrix[5] = 0.0f;
  hasCalibration = true; // Verwende Standard-Kalibrierung
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

  // Hardcoded Touch-Bereiche basierend auf aktuellem 2x4 Layout
  // Ökostrom (Sensor 0) - oben links
  touchAreas[0] = TouchArea(0, 0, 160, 60, 0);

  // Preis (Sensor 1) - oben rechts
  touchAreas[1] = TouchArea(160, 0, 160, 60, 1);

  // Aktie (Sensor 2) - zweite Reihe links
  touchAreas[2] = TouchArea(0, 60, 160, 60, 2);

  // Ladestand (Sensor 3) - zweite Reihe rechts
  touchAreas[3] = TouchArea(160, 60, 160, 60, 3);

  // Verbrauch (Sensor 4) - dritte Reihe links
  touchAreas[4] = TouchArea(0, 120, 160, 60, 4);

  // Wallbox (Sensor 5) - dritte Reihe rechts
  touchAreas[5] = TouchArea(160, 120, 160, 60, 5);

  // Außentemperatur (Sensor 6) - unten links
  touchAreas[6] = TouchArea(0, 180, 160, 60, 6);

  // Wassertemperatur (Sensor 7) - unten rechts
  touchAreas[7] = TouchArea(160, 180, 160, 60, 7);

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
  calibrationMode = true;
  // UI will handle calibration display
}

void TouchManager::stopCalibration() {
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

