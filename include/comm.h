#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include <Arduino.h>
#include "data_structures.h"

// ============ MQTT CONNECTION ============

/**
 * @brief Initialize MQTT client
 * Connects to MQTT broker
 */
void initMQTT();

/**
 * @brief Check if MQTT is connected
 * * @return 1 if connected, 0 if disconnected
 */
int isMQTTConnected();

/**
 * @brief Reconnect to MQTT broker
 * * @return 0 if success, -1 if failed
 */
int reconnectMQTT();

/**
 * @brief Keep MQTT alive and process incoming messages
 */
void loopMQTT();

/**
 * @brief Get MQTT client pointer (for custom operations)
 * * @return Pointer to PubSubClient instance
 */
void* getMQTTClient();

// ============ MQTT PUBLISH OPERATIONS ============

/**
 * @brief Publish rolling token update to MQTT
 * * @param update Pointer to RollingTokenUpdate structure
 * @return 0 if success, -1 if failed
 */
int publishRollingTokenUpdate(RollingTokenUpdate *update);

/**
 * @brief Publish event log to MQTT
 * * @param event Pointer to EventLog structure
 * @return 0 if success, -1 if failed
 */
int publishEventLog(EventLog *event);

/**
 * @brief Publish new UID registration to MQTT
 * * @param entry Pointer to UIDEntry
 * @return 0 if success, -1 if failed
 */
int publishUIDRegistration(UIDEntry *entry);

/**
 * @brief Publish database sync request to MQTT
 * Request server to send current database version
 */
int publishDatabaseSyncRequest();

// ============ MQTT SUBSCRIBE/CALLBACK ============

/**
 * @brief Handle incoming MQTT messages
 * Called when message received
 * * @param topic MQTT topic
 * @param payload Message payload
 * @param length Payload length
 */
void onMQTTMessage(char* topic, byte* payload, unsigned int length);

/**
 * @brief Subscribe to required MQTT topics
 * * @return 0 if success
 */
int subscribeMQTTTopics();

// ============ DATABASE SYNC ============

/**
 * @brief Parse database update from MQTT payload (JSON)
 * * @param json_payload JSON string from MQTT
 * @param db Pointer to UIDDatabase to update
 * @return 0 if success, -1 if parse error
 */
int parseDatabaseFromJSON(const char *json_payload, UIDDatabase *db);

/**
 * @brief Convert UIDDatabase to JSON string for transmission
 * * @param db Pointer to UIDDatabase
 * @param json_buffer Output buffer for JSON string
 * @param buffer_size Maximum buffer size
 * @return 0 if success, -1 if buffer overflow
 */
int databaseToJSON(UIDDatabase *db, char *json_buffer, int buffer_size);

/**
 * @brief Merge incoming database update with local database
 * Uses last_sync timestamp to determine which version is newer
 * * @param local_db Local database
 * @param remote_db Remote database from server
 * @return 0 if success
 */
int mergeDatabaseUpdates(UIDDatabase *local_db, UIDDatabase *remote_db);

// ============ WIFI UTILITIES ============

/**
 * @brief Initialize WiFi connection
 * * @param ssid WiFi SSID
 * @param password WiFi password
 * @return 0 if success, -1 if failed
 */
int initWiFi(const char *ssid, const char *password);

/**
 * @brief Check if WiFi is connected
 * * @return 1 if connected, 0 if disconnected
 */
int isWiFiConnected();

/**
 * @brief Get current WiFi signal strength (RSSI)
 * * @return RSSI value in dBm
 */
int getWiFiSignalStrength();

/**
 * @brief Get ESP32 IP address as string
 * * @return Pointer to static IP string buffer
 */
const char* getESP32IPAddress();

#endif // COMM_H