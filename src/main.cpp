#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <Wire.h>

#include "config.h"
#include "data_structures.h"
#include "tasks.h"
#include "security.h"
#include "comm.h"
#include "spiffs.h"
#include "webServer.h"

// ============ GLOBAL OBJECTS ============

// Queues
QueueHandle_t rfidDataQueue = NULL;
QueueHandle_t rollingTokenQueue = NULL;
QueueHandle_t eventLogQueue = NULL;
QueueHandle_t httpRequestQueue = NULL;

// Semaphores
SemaphoreHandle_t rfidReadSemaphore = NULL;
SemaphoreHandle_t wifiConnectedSemaphore = NULL;

// Mutexes
SemaphoreHandle_t databaseMutex = NULL;
SemaphoreHandle_t serialMutex = NULL;

// Databases & State
UIDDatabase uidDatabase;
SecurityState securityState;

// ============ VOLATILE ISR VARIABLES ============
volatile uint32_t rfidInterruptTime = 0;

// ============ ISR CALLBACK ============

void rfidISR() {
    // Minimal ISR - only give semaphore (deferred processing)
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(rfidReadSemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    rfidInterruptTime = millis();
}

// ============ INITIALIZATION FUNCTIONS ============

void initializeSynchronization() {
    // Create queues
    rfidDataQueue = xQueueCreate(RFID_DATA_QUEUE_SIZE, sizeof(RFIDData));
    rollingTokenQueue = xQueueCreate(ROLLING_TOKEN_QUEUE_SIZE, sizeof(RollingTokenUpdate));
    eventLogQueue = xQueueCreate(EVENT_LOG_QUEUE_SIZE, sizeof(EventLog));
    httpRequestQueue = xQueueCreate(HTTP_REQUEST_QUEUE_SIZE, sizeof(HTTPRequest));

    // Create semaphores
    rfidReadSemaphore = xSemaphoreCreateBinary();
    wifiConnectedSemaphore = xSemaphoreCreateBinary();

    // Create mutexes
    databaseMutex = xSemaphoreCreateMutex();
    serialMutex = xSemaphoreCreateMutex();

    // Verify all objects created
    if (rfidDataQueue == NULL || rollingTokenQueue == NULL || 
        eventLogQueue == NULL || httpRequestQueue == NULL ||
        rfidReadSemaphore == NULL || databaseMutex == NULL || serialMutex == NULL) {
        Serial.println("ERROR: Failed to create FreeRTOS synchronization objects!");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void initializeDatabase() {
    // Initialize security state
    securityState.failed_attempts = 0;
    securityState.is_locked = 0;
    securityState.lockout_until = 0;
    securityState.last_attempt = 0;

    // Initialize UID database
    memset(&uidDatabase, 0, sizeof(UIDDatabase));

    // Load from SPIFFS
    if (loadUIDsFromFile(&uidDatabase, SPIFFS_UID_FILE) == 0) {
        xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
        Serial.printf("[BOOT] Loaded %d UIDs from SPIFFS\n", uidDatabase.count);
        xSemaphoreGive(serialMutex);
    } else {
        xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
        Serial.println("[BOOT] No UID database found, creating empty database");
        xSemaphoreGive(serialMutex);
        uidDatabase.count = 0;
        strcpy(uidDatabase.version, "1.0");
    }
}

void setupHardware() {
    // Setup serial
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== FreeRTOS RFID Attendance System ===");
    Serial.println("Booting...\n");

    // Setup GPIO pins
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(RFID_INT_PIN, INPUT);

    // Initial state: all outputs OFF
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(RELAY_PIN, LOW);

    Serial.println("[BOOT] GPIO initialized");

    // Setup I2C for LCD
    Wire.begin(21, 22);  // SDA=21, SCL=22 (default for ESP32)
    Serial.println("[BOOT] I2C initialized");

    // Setup SPIFFS
    if (initSPIFFS() == 0) {
        Serial.println("[BOOT] SPIFFS mounted successfully");
    } else {
        Serial.println("[ERROR] Failed to mount SPIFFS!");
    }
}

void setupWiFi() {
    Serial.print("[BOOT] Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" OK");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        xSemaphoreGive(wifiConnectedSemaphore);
    } else {
        Serial.println(" FAILED");
        Serial.println("[WARNING] WiFi not connected - some features will be limited");
    }
}

void createAllTasks() {
    // Create Input Task (HIGH priority)
    xTaskCreate(vInputTask, 
                "InputTask", 
                INPUT_TASK_STACK, 
                NULL, 
                INPUT_PRIORITY, 
                NULL);

    // Create Auth Task (HIGH priority)
    xTaskCreate(vAuthTask, 
                "AuthTask", 
                AUTH_TASK_STACK, 
                NULL, 
                AUTH_PRIORITY, 
                NULL);

    // Create Web Server Task (MEDIUM priority)
    xTaskCreate(vWebServerTask, 
                "WebServerTask", 
                WEBSERVER_STACK, 
                NULL, 
                WEBSERVER_PRIORITY, 
                NULL);

    // Create Comm Task (MEDIUM priority)
    xTaskCreate(vCommTask, 
                "CommTask", 
                COMM_TASK_STACK, 
                NULL, 
                COMM_PRIORITY, 
                NULL);

    // Create Security Task (MEDIUM priority)
    xTaskCreate(vSecurityTask, 
                "SecurityTask", 
                SECURITY_TASK_STACK, 
                NULL, 
                SECURITY_PRIORITY, 
                NULL);

    // Create Display Task (LOW priority)
    xTaskCreate(vDisplayTask, 
                "DisplayTask", 
                DISPLAY_TASK_STACK, 
                NULL, 
                DISPLAY_PRIORITY, 
                NULL);

    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
    Serial.println("[BOOT] All 6 FreeRTOS tasks created successfully");
    xSemaphoreGive(serialMutex);
}

void attachISR() {
    // Attach GPIO interrupt
    attachInterrupt(digitalPinToInterrupt(RFID_INT_PIN), rfidISR, FALLING);
    
    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
    Serial.printf("[BOOT] ISR attached to GPIO %d\n", RFID_INT_PIN);
    xSemaphoreGive(serialMutex);
}

// ============ MAIN SETUP ============

void setup() {
    setupHardware();
    setupWiFi();

    // Initialize FreeRTOS synchronization
    initializeSynchronization();

    // Load database from SPIFFS
    initializeDatabase();

    // Initialize MQTT
    initMQTT();

    // Initialize web server
    if (initWebServer() == 0) {
        startWebServer();
        xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
        Serial.printf("[BOOT] Web server started on port %d\n", WEB_SERVER_PORT);
        xSemaphoreGive(serialMutex);
    }

    // Attach ISR
    attachISR();

    // Create all tasks
    createAllTasks();

    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
    Serial.println("[BOOT] Setup complete - FreeRTOS scheduler starting...\n");
    xSemaphoreGive(serialMutex);

    // FreeRTOS scheduler already starts automatically
    // No need for vTaskStartScheduler() - Arduino framework handles this
}

// ============ MAIN LOOP ============

void loop() {
    // With FreeRTOS, the main loop can be used for monitoring or left empty
    // All real work is done by tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Optional: Print task stats every 10 seconds
    static int counter = 0;
    if (++counter >= 10) {
        counter = 0;
        printTaskStats();
    }
}

// ============ UTILITY FUNCTIONS ============

void printTaskStats() {
    xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100));
    
    Serial.println("\n=== TASK STATISTICS ===");
    Serial.printf("WiFi: %s\n", isWiFiConnected() ? "Connected" : "Disconnected");
    Serial.printf("MQTT: %s\n", isMQTTConnected() ? "Connected" : "Disconnected");
    Serial.printf("Web Server: %s\n", isWebServerRunning() ? "Running" : "Stopped");
    Serial.printf("System Locked: %s\n", isSystemLocked(&securityState) ? "YES" : "NO");
    Serial.printf("Failed Attempts: %d/%d\n", securityState.failed_attempts, MAX_FAILED_ATTEMPTS);
    
    Serial.println("=======================\n");
    
    xSemaphoreGive(serialMutex);
}