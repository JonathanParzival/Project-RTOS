#include "tasks.h"
#include "config.h"
#include "spiffs.h"
#include "comm.h"
#include "security.h"
#include <Arduino.h>

void vWebServerTask(void *pvParameters) {
    HTTPRequest req;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
    Serial.println("[WebServer] Task started - handling HTTP requests");
    xSemaphoreGive(serialMutex);

    while (1) {
        // Drain the HTTP queue completely every cycle
        while (xQueueReceive(httpRequestQueue, &req, 0) == pdPASS) {
            if (req.type == HTTP_POST_REGISTER) {
                UIDEntry newEntry;
                memset(&newEntry, 0, sizeof(UIDEntry));
                strncpy(newEntry.uid, req.uid, 9);
                strncpy(newEntry.name, req.name, 50);
                newEntry.timestamp_reg = millis();
                newEntry.rolling_state = 0;

                // Protect database access with Mutex
                if (xSemaphoreTake(databaseMutex, portMAX_DELAY) == pdTRUE) {
                    addUIDToFile(&newEntry, SPIFFS_UID_FILE);
                    loadUIDsFromFile(&uidDatabase, SPIFFS_UID_FILE); // Refresh global state
                    xSemaphoreGive(databaseMutex);
                    
                    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                    Serial.printf("[WebServer] ✓ Registered new UID: %s (%s)\n", newEntry.uid, newEntry.name);
                    xSemaphoreGive(serialMutex);
                }
                
                // Broadcast to MQTT
                publishUIDRegistration(&newEntry);
                
                // Log the event
                EventLog eventLog;
                eventLog.type = EVENT_UID_REGISTERED;
                eventLog.timestamp = millis();
                eventLog.result = 1;
                uidHexToBinary(newEntry.uid, eventLog.uid);
                strcpy(eventLog.user_name, newEntry.name);
                strcpy(eventLog.message, "New UID registered via Web UI");
                xQueueSend(eventLogQueue, &eventLog, pdMS_TO_TICKS(10));
            }
            else if (req.type == HTTP_DELETE_UID) {
                if (xSemaphoreTake(databaseMutex, portMAX_DELAY) == pdTRUE) {
                    deleteUIDFromFile(req.uid, SPIFFS_UID_FILE);
                    loadUIDsFromFile(&uidDatabase, SPIFFS_UID_FILE); // Refresh global state
                    xSemaphoreGive(databaseMutex);
                    
                    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                    Serial.printf("[WebServer] ✓ Deleted UID: %s\n", req.uid);
                    xSemaphoreGive(serialMutex);
                }
            }
        }
        
        // Wait for next cycle
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(WEBSERVER_PERIOD));
    }
}