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

TouchManager::TouchManager() : isInitialized(false), calibrationMode(false), hasCalibration(false) {
  // Initialize calibration matrix with identity transform
  calibrationMatrix[0] = 1.0f; calibrationMatrix[1] = 0.0f; calibrationMatrix[2] = 0.0f;
  calibrationMatrix[3] = 0.0f; calibrationMatrix[4] = 1.0f; calibrationMatrix[5] = 0.0f;
}

bool TouchManager::initialize() {
  Serial.println("🎯 Initialisiere Touch-Controller CST820...");

  // Initialize I2C pins for CST820
  if (!touch.init(TouchConfig::SDA_PIN, TouchConfig::SCL_PIN,
                  TouchConfig::RESET_PIN, TouchConfig::INT_PIN)) {
    Serial.println("❌ CST820 Touch-Controller nicht gefunden!");
    return false;
  }

  // Set display orientation to match current display rotation
  touch.setOrientation(1, TouchConfig::DISPLAY_WIDTH, TouchConfig::DISPLAY_HEIGHT);

  // Load calibration data from EEPROM
  loadCalibration();

  // Initialize touch areas based on current sensor layout
  updateSensorTouchAreas();

  isInitialized = true;
  Serial.printf("✅ Touch-Controller initialisiert (Typ: %d)\n", touch.sensorType());

  if (hasCalibration) {
    Serial.println("📏 Kalibrierungsdaten geladen");
  } else {
    Serial.println("⚠️ Keine Kalibrierung - verwende Standard-Transformation");
  }

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

  TOUCHINFO touchInfo;
  int touchCount = touch.getSamples(&touchInfo);

  unsigned long now = millis();

  if (touchCount > 0 && touchInfo.count > 0) {
    // Create touch point from first touch
    TouchPoint newPoint(touchInfo.x[0], touchInfo.y[0],
                       touchInfo.pressure[0], touchInfo.area[0]);

    // Apply calibration transformation
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

      Serial.printf("👆 Touch DOWN (%d,%d) Sensor:%d\n",
                   newPoint.x, newPoint.y, event.sensorIndex);

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

        Serial.printf("📌 Long Press (%d,%d) Sensor:%d\n",
                     state.currentPoint.x, state.currentPoint.y, event.sensorIndex);
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
        Serial.printf("👆👆 Double Tap (%d,%d) Sensor:%d\n",
                     state.currentPoint.x, state.currentPoint.y, event.sensorIndex);
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
        Serial.printf("👋 Gesture: %s\n", touchEventToString(gesture).c_str());
      } else {
        event.type = TOUCH_UP;
      }
    }

    if (event.type == TOUCH_UP) {
      Serial.printf("👆 Touch UP (%d,%d) Sensor:%d Duration:%lums\n",
                   state.currentPoint.x, state.currentPoint.y, event.sensorIndex, pressDuration);
    }
  }

  // Reset tap count after timeout
  if (state.tapCount > 0 && (now - state.lastEventTime) > TouchConfig::DOUBLE_TAP_MS) {
    state.tapCount = 0;
  }

  return event;
}

void TouchManager::updateSensorTouchAreas() {
  if (!isInitialized) return;

  Serial.println("🔧 Aktualisiere Touch-Bereiche...");

  for (int i = 0; i < System::SENSOR_COUNT; i++) {
    const SensorData& sensor = sensors[i];

    // Add anti-burnin offset
    int offsetX = antiBurnin.getOffsetX();

    touchAreas[i] = TouchArea(
      sensor.layout.x + offsetX,
      sensor.layout.y,
      sensor.layout.w,
      sensor.layout.h,
      i
    );

    Serial.printf("   Touch[%d]: (%d,%d) %dx%d\n",
                 i, touchAreas[i].x, touchAreas[i].y,
                 touchAreas[i].width, touchAreas[i].height);
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

void TouchManager::applyCalibration(TouchPoint& point) {
  if (!hasCalibration) return;

  float x = point.x;
  float y = point.y;

  // Apply 2D transformation matrix
  point.x = (uint16_t)(calibrationMatrix[0] * x + calibrationMatrix[1] * y + calibrationMatrix[2]);
  point.y = (uint16_t)(calibrationMatrix[3] * x + calibrationMatrix[4] * y + calibrationMatrix[5]);

  // Clamp to display bounds
  point.x = constrain(point.x, 0, TouchConfig::DISPLAY_WIDTH - 1);
  point.y = constrain(point.y, 0, TouchConfig::DISPLAY_HEIGHT - 1);
}

void TouchManager::startCalibration() {
  Serial.println("🎯 Starte Touch-Kalibrierung...");
  calibrationMode = true;
  // UI will handle calibration display
}

void TouchManager::stopCalibration() {
  calibrationMode = false;
  saveCalibration();
  Serial.println("✅ Touch-Kalibrierung abgeschlossen");
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
  Serial.println("📖 Lade Kalibrierungsdaten...");

  int addr = EEPROM_CALIBRATION_START;
  int magic;
  EEPROM.get(addr, magic);

  if (magic != EEPROM_CALIBRATION_MAGIC) {
    Serial.println("⚠️ Keine gültigen Kalibrierungsdaten gefunden");
    hasCalibration = false;
    return;
  }

  addr += sizeof(int);
  for (int i = 0; i < 6; i++) {
    EEPROM.get(addr, calibrationMatrix[i]);
    addr += sizeof(float);
  }

  hasCalibration = true;
  Serial.println("✅ Kalibrierung geladen");
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

  Serial.printf("🎯 Sensor %d (%s) berührt\n", sensorIndex, sensors[sensorIndex].label);

  // Add visual feedback
  renderManager.markSensorChanged(sensorIndex);

  // Future: Add sensor-specific actions
  // - Toggle sensor details
  // - Open sensor history
  // - Sensor-specific settings
}

void onGestureDetected(TouchEventType gesture) {
  switch (gesture) {
    case SWIPE_LEFT:
      Serial.println("👈 Swipe Left - Nächster Modus");
      // Future: Switch to next display mode
      break;

    case SWIPE_RIGHT:
      Serial.println("👉 Swipe Right - Vorheriger Modus");
      // Future: Switch to previous display mode
      break;

    case SWIPE_UP:
      Serial.println("👆 Swipe Up - Details anzeigen");
      // Future: Show detailed view
      break;

    case SWIPE_DOWN:
      Serial.println("👇 Swipe Down - Zurück zur Hauptansicht");
      // Future: Return to main view
      break;

    default:
      break;
  }
}

void onLongPress(const TouchPoint& point) {
  Serial.printf("📌 Long Press bei (%d,%d)\n", point.x, point.y);

  // Check if long press is in empty area (not on sensor)
  int sensorIndex = touchManager.findTouchedSensor(point);
  if (sensorIndex == -1) {
    // Long press in empty area - start calibration
    Serial.println("🎯 Starte Kalibrierung...");
    touchManager.startCalibration();
  } else {
    // Long press on sensor - sensor-specific action
    Serial.printf("🔧 Sensor %d Einstellungen\n", sensorIndex);
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