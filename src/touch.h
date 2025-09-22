#pragma once

#include <Arduino.h>
#include <bb_captouch.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════════════════════
//                              TOUCH CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

namespace TouchConfig {
  // JC2432W328 CST820 Pin Configuration
  const int SDA_PIN = 33;
  const int SCL_PIN = 32;
  const int INT_PIN = 21;
  const int RESET_PIN = 25;

  // Touch Settings
  const int MAX_TOUCH_POINTS = 5;
  const int TOUCH_THRESHOLD = 10;  // Minimum movement for valid touch
  const unsigned long DEBOUNCE_MS = 50;
  const unsigned long LONG_PRESS_MS = 1000;
  const unsigned long DOUBLE_TAP_MS = 300;

  // Display Orientation
  const int DISPLAY_WIDTH = 320;
  const int DISPLAY_HEIGHT = 240;
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              TOUCH DATA STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════════

enum TouchEventType {
  TOUCH_NONE,
  TOUCH_DOWN,
  TOUCH_MOVE,
  TOUCH_UP,
  TOUCH_LONG_PRESS,
  TOUCH_DOUBLE_TAP,
  SWIPE_LEFT,
  SWIPE_RIGHT,
  SWIPE_UP,
  SWIPE_DOWN
};

struct TouchPoint {
  uint16_t x, y;
  uint8_t pressure;
  uint8_t area;
  bool isValid;

  TouchPoint() : x(0), y(0), pressure(0), area(0), isValid(false) {}
  TouchPoint(uint16_t x, uint16_t y, uint8_t p = 0, uint8_t a = 0)
    : x(x), y(y), pressure(p), area(a), isValid(true) {}
};

struct TouchEvent {
  TouchEventType type;
  TouchPoint point;
  TouchPoint startPoint;  // For gestures
  unsigned long timestamp;
  int sensorIndex;        // Which sensor box was touched (-1 if none)

  TouchEvent() : type(TOUCH_NONE), timestamp(0), sensorIndex(-1) {}
};

struct TouchState {
  bool isPressed;
  TouchPoint currentPoint;
  TouchPoint lastPoint;
  TouchPoint startPoint;
  unsigned long pressStartTime;
  unsigned long lastEventTime;
  int tapCount;
  bool longPressTriggered;

  TouchState() : isPressed(false), pressStartTime(0), lastEventTime(0),
                 tapCount(0), longPressTriggered(false) {}
};

struct TouchArea {
  int x, y, width, height;
  int sensorIndex;
  bool isActive;

  TouchArea() : x(0), y(0), width(0), height(0), sensorIndex(-1), isActive(true) {}
  TouchArea(int x, int y, int w, int h, int index)
    : x(x), y(y), width(w), height(h), sensorIndex(index), isActive(true) {}

  bool contains(const TouchPoint& point) const {
    return point.isValid &&
           point.x >= x && point.x < (x + width) &&
           point.y >= y && point.y < (y + height) &&
           isActive;
  }
};

// ═══════════════════════════════════════════════════════════════════════════════
//                              TOUCH MANAGER CLASS
// ═══════════════════════════════════════════════════════════════════════════════

class TouchManager {
private:
  BBCapTouch touch;
  TouchState state;
  TouchArea touchAreas[System::SENSOR_COUNT];
  bool isInitialized;
  bool calibrationMode;

  // Calibration data
  float calibrationMatrix[6];  // Touch to screen transformation matrix
  bool hasCalibration;

  // Internal methods
  void updateTouchAreas();
  TouchEventType detectGesture(const TouchPoint& start, const TouchPoint& end, unsigned long duration);
  void applyCalibration(TouchPoint& point);
  void saveCalibration();
  void loadCalibration();

public:
  TouchManager();

  // Initialization
  bool initialize();
  void reset();

  // Main update loop
  TouchEvent update();

  // Calibration
  void startCalibration();
  void stopCalibration();
  bool isCalibrating() const { return calibrationMode; }
  void setCalibrationPoint(int index, const TouchPoint& screenPoint, const TouchPoint& touchPoint);

  // Touch area management
  void updateSensorTouchAreas();
  void enableTouchArea(int sensorIndex, bool enable = true);
  bool isTouchAreaEnabled(int sensorIndex) const;

  // Getters
  bool isTouch() const { return state.isPressed; }
  TouchPoint getCurrentTouch() const { return state.currentPoint; }
  bool hasValidCalibration() const { return hasCalibration; }
  int findTouchedSensor(const TouchPoint& point);

  // Debug
  void printTouchInfo(const TouchEvent& event);
  void printCalibrationInfo();
};

// ═══════════════════════════════════════════════════════════════════════════════
//                              GLOBAL TOUCH FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// Global touch manager instance
extern TouchManager touchManager;

// Initialization
bool initializeTouch();

// Main touch processing
TouchEvent processTouchInput();

// UI Navigation callbacks
void onSensorTouched(int sensorIndex);
void onGestureDetected(TouchEventType gesture);
void onLongPress(const TouchPoint& point);

// Helper functions
String touchEventToString(TouchEventType type);
bool isTouchInSensorArea(const TouchPoint& point, int sensorIndex);