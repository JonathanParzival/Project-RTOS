#ifndef TASKS_H
#define TASKS_H

#include <Arduino.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
}

#include "data_structures.h"

// ============ EXTERNAL OBJECTS (defined in main.cpp) ============
extern QueueHandle_t rfidDataQueue;
extern QueueHandle_t rollingTokenQueue;
extern QueueHandle_t eventLogQueue;
extern QueueHandle_t httpRequestQueue;

extern SemaphoreHandle_t rfidReadSemaphore;
extern SemaphoreHandle_t wifiConnectedSemaphore;

extern SemaphoreHandle_t databaseMutex;
extern SemaphoreHandle_t serialMutex;

extern UIDDatabase uidDatabase;
extern SecurityState securityState;

// ============ TASK FUNCTION DECLARATIONS ============

/**
 * @brief Input Task - Listen to RFID reader and queue detected UIDs
 * Priority: HIGH (3)
 * Period: 50ms
 */
void vInputTask(void *pvParameters);

/**
 * @brief Authentication Task - Verify UID and calculate rolling token
 * Priority: HIGH (3)
 * Period: 100ms
 */
void vAuthTask(void *pvParameters);

/**
 * @brief Web Server Task - Handle HTTP registration and management
 * Priority: MEDIUM (2)
 * Period: ~100ms event-driven
 */
void vWebServerTask(void *pvParameters);

/**
 * @brief Communication Task - Sync with MQTT broker
 * Priority: MEDIUM (2)
 * Period: 500ms
 */
void vCommTask(void *pvParameters);

/**
 * @brief Security Task - Monitor failed attempts and lockout
 * Priority: MEDIUM (2)
 * Period: 1000ms
 */
void vSecurityTask(void *pvParameters);

/**
 * @brief Display Task - Update LCD and Serial output
 * Priority: LOW (1)
 * Period: 250ms
 */
void vDisplayTask(void *pvParameters);

// ============ ISR CALLBACK ============

/**
 * @brief ISR for RFID reader interrupt
 * Minimal code - only give semaphore
 */
void rfidISR();

// ============ UTILITY FUNCTIONS ============

/**
 * @brief Initialize all synchronization objects (semaphore, queue, mutex)
 */
void initializeSynchronization();

/**
 * @brief Create all 6 FreeRTOS tasks
 */
void createAllTasks();

/**
 * @brief Print task statistics for debugging
 */
void printTaskStats();

#endif // TASKS_H
