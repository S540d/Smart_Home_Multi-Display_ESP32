#ifndef STATE_H
#define STATE_H

#include "config.h"
#include <freertos/semphr.h>

// Central application state — single owner, all modules access via getInstance().
class SystemState {
public:
  // Sensor data
  SensorData sensors[System::SENSOR_COUNT];
  SystemStatus systemStatus;
  AntiBurninManager antiBurnin;
  RenderManager renderManager;

  // Stock data
  float stockReference = 0.0f;
  float stockPreviousClose = 0.0f;

  // Power management (kW, always positive; direction via isGridFeedIn/isStorageCharging)
  float pvPower = 0.0f;
  float gridPower = 0.0f;
  float loadPower = 0.0f;
  float storagePower = 0.0f;
  float wallboxPower = 0.0f;
  LastValidPowerValues lastValidPower;
  bool isGridFeedIn = false;
  bool isStorageCharging = false;

  // SmartPlug states
  SmartPlugState smartPlugs[SMART_PLUG_COUNT];

  // Calendar (next upcoming event, pre-formatted text from NodeRed)
  CalendarState calendar;

  // Car (SoC + state from TeslaMate via MQTT)
  CarState car;

  // Garage door (open/closed from Node-RED, full-screen alert while open)
  GarageState garage;

  // Air quality (eCO2 from Wemos sensor, full-screen alert when ventilation needed)
  AirQualityState air;

  // Timing
  unsigned long lastSystemUpdate = 0;
  unsigned long lastTimeoutCheck = 0;
  unsigned long lastWifiReconnect = 0;
  unsigned long lastLDROutput = 0;
  unsigned long systemStartTime = 0;

  // Touch init (delayed to avoid bootloop)
  bool touchInitAttempted = false;
  unsigned long touchInitDelay = 10000;

  // CPU frequency scaling
  unsigned long lastActivityTime = 0;
  bool cpuIsIdle = false;

  // Display mode
  DisplayMode currentMode = HOME_SCREEN;
  unsigned long lastViewChangeTime = 0;

  // Day-ahead price data
  DayAheadPriceData dayAheadPrices;

  // Mutex for thread safety — initialised in begin(), used by FreeRTOS tasks (Issue #23)
  SemaphoreHandle_t stateMutex = nullptr;

  // Call once from setup() after FreeRTOS scheduler is running
  void begin() {
    stateMutex = xSemaphoreCreateRecursiveMutex();
    if (stateMutex == nullptr) {
      Serial.println("FATAL: stateMutex konnte nicht erstellt werden");
      esp_restart();
    }
  }

  static SystemState& getInstance() {
    static SystemState instance;
    return instance;
  }

  SystemState(const SystemState&) = delete;
  SystemState& operator=(const SystemState&) = delete;

private:
  SystemState() = default;
};

#endif // STATE_H
