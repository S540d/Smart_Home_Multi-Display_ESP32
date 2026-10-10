#ifndef TASKS_H
#define TASKS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "touch.h"

// Inter-task queues
extern QueueHandle_t touchEventQueue;   // touchTask → displayTask
extern QueueHandle_t plugCommandQueue;  // displayTask → networkTask

// Task handles
extern TaskHandle_t touchTaskHandle;
extern TaskHandle_t networkTaskHandle;
extern TaskHandle_t displayTaskHandle;
extern TaskHandle_t systemTaskHandle;

// MQTT publish command sent from display/touch context to networkTask
struct PlugCommand {
  int plugIndex;
  bool newState;
};

// Functions defined in main.cpp, called from task implementations
extern void handleTouchEvent(const TouchEvent& event);
extern void notifyActivity();
extern void updateSystemStatus();
extern void handleLowMemory();
extern void updateCpuScaling();
extern void publishPlugCommandDirect(int plugIndex, bool newState);

// Task entry points
void touchTask(void* parameter);
void networkTask(void* parameter);
void displayTask(void* parameter);
void systemTask(void* parameter);

// Called at end of setup() after all hardware is initialized
void initializeTasks();

#endif // TASKS_H
