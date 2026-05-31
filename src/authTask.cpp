#include "tasks.h"
#include "config.h"
#include "security.h"
#include "spiffs.h"
#include <Arduino.h>

void vAuthTask(void *pvParameters) {
    RFIDData rfidData;
    RollingTokenUpdate rollingUpdate;
    EventLog eventLog;
    UIDEntry matchedEntry;
    BaseType_t xStatus;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int uid_index;

    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
    Serial.println("[Auth] Task started - verifying UIDs...");
    xSemaphoreGive(serialMutex);

    while (1) {
        xStatus = xQueueReceive(rfidDataQueue, &rfidData, pdMS_TO_TICKS(100));

        if (xStatus == pdPASS) {
            if (xSemaphoreTake(databaseMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                
                uid_index = verifyUID(rfidData.uid, &uidDatabase, &matchedEntry);

                if (uid_index >= 0) {
                    uint8_t new_uid[4];
                    
                    if (calculateRollingToken(rfidData.uid, new_uid) == 0) {
                        if (updateUIDInDatabase(&uidDatabase, uid_index, new_uid) == 0) {
                            saveUIDsToFile(&uidDatabase, SPIFFS_UID_FILE);

                            xSemaphoreGive(databaseMutex);

                            feedbackSuccess();

                            rollingUpdate.timestamp = rfidData.timestamp;
                            uidCopy(rfidData.uid, rollingUpdate.old_uid);
                            uidCopy(new_uid, rollingUpdate.new_uid);
                            strcpy(rollingUpdate.user_id, matchedEntry.name);

                            xQueueSend(rollingTokenQueue, &rollingUpdate, pdMS_TO_TICKS(10));

                            eventLog.type = EVENT_ACCESS_GRANTED;
                            eventLog.timestamp = rfidData.timestamp;
                            eventLog.result = 1;
                            uidCopy(rfidData.uid, eventLog.uid);
                            strcpy(eventLog.user_name, matchedEntry.name);
                            strcpy(eventLog.message, "Access granted - Rolling Token updated");

                            xQueueSend(eventLogQueue, &eventLog, pdMS_TO_TICKS(10));

                            xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                            Serial.printf("[Auth] ✓ ACCESS GRANTED: %s\n", matchedEntry.name);
                            Serial.printf("      Old UID: %02X%02X%02X%02X\n", 
                                          rfidData.uid[0], rfidData.uid[1],
                                          rfidData.uid[2], rfidData.uid[3]);
                            Serial.printf("      New UID: %02X%02X%02X%02X\n",
                                          new_uid[0], new_uid[1], new_uid[2], new_uid[3]);
                            xSemaphoreGive(serialMutex);

                            clearFailedAttempts(&securityState);

                        } else {
                            xSemaphoreGive(databaseMutex);
                            feedbackFailure();
                            
                            eventLog.type = EVENT_ERROR;
                            eventLog.timestamp = rfidData.timestamp;
                            eventLog.result = 0;
                            uidCopy(rfidData.uid, eventLog.uid);
                            strcpy(eventLog.message, "Error updating database");
                            xQueueSend(eventLogQueue, &eventLog, pdMS_TO_TICKS(10));
                        }
                    } else {
                        xSemaphoreGive(databaseMutex);
                        feedbackFailure();
                        
                        eventLog.type = EVENT_ERROR;
                        eventLog.timestamp = rfidData.timestamp;
                        eventLog.result = 0;
                        uidCopy(rfidData.uid, eventLog.uid);
                        strcpy(eventLog.message, "Error calculating rolling token");
                        xQueueSend(eventLogQueue, &eventLog, pdMS_TO_TICKS(10));
                    }
                } else {
                    xSemaphoreGive(databaseMutex);
                    feedbackFailure();

                    int attempt_count = recordFailedAttempt(&securityState);

                    eventLog.type = EVENT_SPOOFING_DETECTED;
                    eventLog.timestamp = rfidData.timestamp;
                    eventLog.result = 0;
                    uidCopy(rfidData.uid, eventLog.uid);
                    strcpy(eventLog.user_name, "Unknown");
                    snprintf(eventLog.message, 100, 
                             "UID not found or spoofed (attempt %d/%d)",
                             attempt_count, MAX_FAILED_ATTEMPTS);

                    xQueueSend(eventLogQueue, &eventLog, pdMS_TO_TICKS(10));

                    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                    Serial.printf("[Auth] ✗ ACCESS DENIED: UID %02X%02X%02X%02X not found\n",
                                  rfidData.uid[0], rfidData.uid[1],
                                  rfidData.uid[2], rfidData.uid[3]);
                    Serial.printf("      Failed attempt: %d/%d\n", attempt_count, MAX_FAILED_ATTEMPTS);
                    xSemaphoreGive(serialMutex);

                    if (isSystemLocked(&securityState)) {
                        feedbackLockout();
                        
                        eventLog.type = EVENT_SYSTEM_LOCKOUT;
                        eventLog.timestamp = millis();
                        eventLog.result = 0;
                        strcpy(eventLog.message, "System locked - too many failed attempts");
                        xQueueSend(eventLogQueue, &eventLog, pdMS_TO_TICKS(10));

                        xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                        Serial.println("[Auth] ⚠ SYSTEM LOCKED!");
                        xSemaphoreGive(serialMutex);
                    }
                }
            } else {
                xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
                Serial.println("[Auth] WARNING: Mutex timeout");
                xSemaphoreGive(serialMutex);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(AUTH_TASK_PERIOD));
    }
}