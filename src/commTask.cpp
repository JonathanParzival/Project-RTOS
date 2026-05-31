#include "tasks.h"
#include "config.h"
#include "comm.h"
#include <Arduino.h>
#include <ArduinoJson.h>

void vCommTask(void *pvParameters) {
    RollingTokenUpdate rollingUpdate;
    EventLog eventLog;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static uint32_t last_sync_time = 0;
    const uint32_t SYNC_INTERVAL = 5000;  // Sync every 5 seconds

    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
    Serial.println("[Comm] Task started - MQTT communication ready");
    xSemaphoreGive(serialMutex);

    while (1) {
        uint32_t current_time = millis();

        // Ensure MQTT connection
        if (!isMQTTConnected()) {
            if (reconnectMQTT() != 0) {
                xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                Serial.println("[Comm] WARNING: MQTT not connected");
                xSemaphoreGive(serialMutex);
            }
        } else {
            // Keep MQTT client alive and process incoming messages
            loopMQTT(); 
        }

        // Drain rolling token queue completely
        while (xQueueReceive(rollingTokenQueue, &rollingUpdate, 0) == pdPASS) {
            if (isMQTTConnected()) {
                if (publishRollingTokenUpdate(&rollingUpdate) == 0) {
                    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                    Serial.printf("[Comm] ✓ Published rolling token update for %s\n", rollingUpdate.user_id);
                    xSemaphoreGive(serialMutex);
                } else {
                    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                    Serial.println("[Comm] WARNING: Failed to publish rolling token update");
                    xSemaphoreGive(serialMutex);
                }
            }
        }

        // Drain event log queue completely
        while (xQueueReceive(eventLogQueue, &eventLog, 0) == pdPASS) {
            if (isMQTTConnected()) {
                if (publishEventLog(&eventLog) == 0) {
                    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                    Serial.printf("[Comm] ✓ Published event: %s\n", eventLog.message);
                    xSemaphoreGive(serialMutex);
                }
            }
        }

        // Periodically sync database with server
        if (current_time - last_sync_time > SYNC_INTERVAL) {
            last_sync_time = current_time;

            if (isMQTTConnected()) {
                xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                Serial.println("[Comm] Requesting database sync from server...");
                xSemaphoreGive(serialMutex);

                if (publishDatabaseSyncRequest() == 0) {
                    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                    Serial.println("[Comm] Sync request sent");
                    xSemaphoreGive(serialMutex);
                }
            }
        }

        // Keep-alive: publish periodic status
        static uint32_t last_status_time = 0;
        if (current_time - last_status_time > 30000) {  // Every 30 seconds
            last_status_time = current_time;

            if (isMQTTConnected()) {
                xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                Serial.println("[Comm] System status OK");
                xSemaphoreGive(serialMutex);
            }
        }

        // Periodic delay 500ms
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(COMM_TASK_PERIOD));
    }
}