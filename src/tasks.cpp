#include "tasks.h"
#include "state.h"
#include "network.h"
#include "display.h"
#include "sensors.h"
#include "utils.h"
#include "ota.h"
#include "touch.h"
#include <esp_task_wdt.h>

static SystemState& appState = SystemState::getInstance();

QueueHandle_t touchEventQueue  = nullptr;
QueueHandle_t plugCommandQueue = nullptr;
TaskHandle_t  touchTaskHandle   = nullptr;
TaskHandle_t  networkTaskHandle = nullptr;
TaskHandle_t  displayTaskHandle = nullptr;
TaskHandle_t  systemTaskHandle  = nullptr;

extern PubSubClient client;

// ─── Touch Task (Core 1, Priority 4) ────────────────────────────────────────
void touchTask(void* parameter) {
  esp_task_wdt_add(NULL);
  for (;;) {
    esp_task_wdt_reset();

    bool calibrating = false;
    if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      calibrating = touchManager.isCalibrating();
      xSemaphoreGiveRecursive(appState.stateMutex);
    }

    if (calibrating) {
      if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        bool updated = touchManager.updateCalibration();
        if (updated) appState.renderManager.markFullRedrawRequired();
        xSemaphoreGiveRecursive(appState.stateMutex);
      }
    } else {
      // Protect touchAreas[] against concurrent updateSensorTouchAreas() in displayTask
      if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        TouchEvent event = processTouchInput();
        xSemaphoreGiveRecursive(appState.stateMutex);
        if (event.type != TOUCH_NONE) {
          notifyActivity();
          xQueueSend(touchEventQueue, &event, pdMS_TO_TICKS(10));
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ─── Network Task (Core 0, Priority 3) ──────────────────────────────────────
void networkTask(void* parameter) {
  esp_task_wdt_add(NULL);
  for (;;) {
    esp_task_wdt_reset();

    handleOTA();

    checkWifiConnection();

    if (!appState.systemStatus.wifiConnected) {
      reconnectWiFi();
    } else {
      if (!client.connected()) {
        reconnectMQTT();
      } else {
        client.loop();
      }

      if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        appState.systemStatus.mqttConnected = client.connected();
        appState.systemStatus.wifiRSSI = WiFi.RSSI();
        xSemaphoreGiveRecursive(appState.stateMutex);
      }
    }

    // Process outbound MQTT publish commands queued by displayTask
    PlugCommand cmd;
    while (xQueueReceive(plugCommandQueue, &cmd, 0) == pdTRUE) {
      publishPlugCommandDirect(cmd.plugIndex, cmd.newState);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ─── Display Task (Core 1, Priority 2) ──────────────────────────────────────
void displayTask(void* parameter) {
  esp_task_wdt_add(NULL);
  for (;;) {
    esp_task_wdt_reset();
    unsigned long now = millis();

    // Process touch events from queue
    TouchEvent event;
    while (xQueueReceive(touchEventQueue, &event, 0) == pdTRUE) {
      if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        handleTouchEvent(event);
        xSemaphoreGiveRecursive(appState.stateMutex);
      }
    }

    // Anti-burnin management
    if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      appState.antiBurnin.update();
      bool offsetChanged = appState.antiBurnin.hasOffsetChanged();
      if (offsetChanged) appState.renderManager.markAntiBurninChanged();
      xSemaphoreGiveRecursive(appState.stateMutex);
      if (offsetChanged) {
        Serial.println("Anti-Burnin: Touch-Bereiche aktualisiert");
        // Protect touchAreas[] against concurrent processTouchInput() in touchTask
        if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          touchManager.updateSensorTouchAreas();
          xSemaphoreGiveRecursive(appState.stateMutex);
        }
      }
    }

    // Auto-return to home screen
    if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      if (appState.currentMode != HOME_SCREEN && appState.lastViewChangeTime > 0 &&
          now - appState.lastViewChangeTime >= Timing::DETAIL_SCREEN_TIMEOUT_MS) {
        appState.currentMode = HOME_SCREEN;
        appState.lastViewChangeTime = 0;
        appState.renderManager.markFullRedrawRequired();
        Serial.println("Auto-Return: HOME_SCREEN");
      }
      bool needsUpdate = appState.renderManager.needsUpdate();
      xSemaphoreGiveRecursive(appState.stateMutex);

      if (needsUpdate) {
        // updateDisplay() reads appState without mutex (acceptable: brief stale data
        // on a display is harmless). clearAllFlags() inside may race with concurrent
        // flag sets, causing one missed 30 ms frame at most before the next change
        // triggers a re-render.
        updateDisplay();
        if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          appState.renderManager.lastRenderUpdate = now;
          xSemaphoreGiveRecursive(appState.stateMutex);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(30));
  }
}

// ─── System Task (Core 0, Priority 1) ───────────────────────────────────────
void systemTask(void* parameter) {
  esp_task_wdt_add(NULL);
  static unsigned long lastReport = 0;
  for (;;) {
    esp_task_wdt_reset();
    unsigned long now = millis();

    // Periodic system metrics update (memory, time, LDR, CPU sim)
    bool doUpdate = false;
    if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (now - appState.lastSystemUpdate >= Timing::SYSTEM_UPDATE_INTERVAL) {
        appState.lastSystemUpdate = now;
        doUpdate = true;
      }
      xSemaphoreGiveRecursive(appState.stateMutex);
    }
    if (doUpdate) {
      if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        updateSystemStatus();
        xSemaphoreGiveRecursive(appState.stateMutex);
      }
    }

    // Sensor timeout check
    bool doTimeoutCheck = false;
    if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (now - appState.lastTimeoutCheck >= Timing::TIMEOUT_CHECK_INTERVAL) {
        appState.lastTimeoutCheck = now;
        doTimeoutCheck = true;
      }
      xSemaphoreGiveRecursive(appState.stateMutex);
    }
    if (doTimeoutCheck) {
      if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        checkSensorTimeouts();
        xSemaphoreGiveRecursive(appState.stateMutex);
      }
    }

    // CPU frequency scaling
    if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      updateCpuScaling();
      xSemaphoreGiveRecursive(appState.stateMutex);
    }

    // Low-memory emergency handling
    bool criticalMem = false;
    if (xSemaphoreTakeRecursive(appState.stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      criticalMem = appState.systemStatus.criticalMemoryWarning;
      xSemaphoreGiveRecursive(appState.stateMutex);
    }
    if (criticalMem) handleLowMemory();

    // 10-minute performance reports
    if (now - lastReport >= 600000) {
      lastReport = now;
      updateSensorPerformance();
      logPerformanceStats();
      logSystemHealth();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ─── Task Initialization ─────────────────────────────────────────────────────
void initializeTasks() {
  touchEventQueue  = xQueueCreate(10, sizeof(TouchEvent));
  plugCommandQueue = xQueueCreate(5,  sizeof(PlugCommand));
  if (touchEventQueue == nullptr || plugCommandQueue == nullptr) {
    Serial.println("FATAL: Queue-Erstellung fehlgeschlagen");
    esp_restart();
  }

  BaseType_t res;
  res = xTaskCreatePinnedToCore(touchTask,   "TouchTask",   4096, nullptr, 4, &touchTaskHandle,   1);
  if (res != pdPASS) { Serial.println("FATAL: TouchTask fehlgeschlagen");   esp_restart(); }
  res = xTaskCreatePinnedToCore(networkTask, "NetworkTask", 8192, nullptr, 3, &networkTaskHandle, 0);
  if (res != pdPASS) { Serial.println("FATAL: NetworkTask fehlgeschlagen"); esp_restart(); }
  res = xTaskCreatePinnedToCore(displayTask, "DisplayTask", 8192, nullptr, 2, &displayTaskHandle, 1);
  if (res != pdPASS) { Serial.println("FATAL: DisplayTask fehlgeschlagen"); esp_restart(); }
  res = xTaskCreatePinnedToCore(systemTask,  "SystemTask",  4096, nullptr, 1, &systemTaskHandle,  0);
  if (res != pdPASS) { Serial.println("FATAL: SystemTask fehlgeschlagen");  esp_restart(); }

  Serial.println("FreeRTOS Tasks gestartet: Touch(P4/C1), Network(P3/C0), Display(P2/C1), System(P1/C0)");
}
