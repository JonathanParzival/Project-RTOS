#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include "data_structures.h"

// ============ WIFI ============

/**
 * @brief Connect to WiFi using config.h credentials
 *
 * Fungsi blocking dengan timeout internal; panggil dari setup/CommTask, bukan ISR.
 *
 * @return 0 if connected, -1 if timeout
 */
int initWiFi(const char *ssid, const char *password);

/**
 * @brief 1 if connected, 0 otherwise
 *
 * Wrapper tipis agar task tidak bergantung langsung pada WiFi.status().
 */
int isWiFiConnected();

/**
 * @brief RSSI in dBm
 *
 * Mengembalikan nilai fallback lemah saat WiFi putus.
 */
int getWiFiSignalStrength();

/**
 * @brief Static string with current IP address
 *
 * Pointer valid sampai pemanggilan berikutnya; jangan dimodifikasi caller.
 */
const char* getESP32IPAddress();

// ============ XAMPP HTTP API ============

/**
 * @brief Verify a scanned UID against the XAMPP MySQL database.
 *
 * POST /api/verify.php   { "uid": "12345678" }
 * Expected response:
 *   { "authorized": true,  "user_id": 3, "user_name": "John Doe" }
 *   { "authorized": false, "user_id": 0, "user_name": "" }
 *
 * @param uid_hex  Null-terminated 8-char hex string (e.g. "12345678")
 * @param response Caller-allocated struct to receive result
 * @return 0 if HTTP call succeeded (check response->authorized for access result),
 *        -1 if network/server error
 */
int verifyUIDWithServer(const char *uid_hex, ServerAuthResponse *response);

/**
 * @brief Tell the server to update the stored UID to the new rolling token value.
 *
 * POST /api/update_uid.php  { "user_id": 3, "new_uid": "1234567A" }
 *
 * @param user_id  Row ID returned by verifyUIDWithServer
 * @param new_uid_hex  New 8-char hex UID after rolling-token step
 * @return 0 on success, -1 on error
 */
int updateUIDOnServer(int user_id, const char *new_uid_hex);

/**
 * @brief Send an access event log entry to the server.
 *
 * POST /api/log_event.php  { "type": 0, "uid": "12345678",
 *                            "user_name": "...", "result": 1,
 *                            "message": "..." }
 *
 * Non-blocking best-effort: a failure here is logged to Serial only.
 * Caller tetap harus siap bila fungsi ini blocking sampai HTTP_TIMEOUT_MS.
 *
 * @param event  Pointer to populated EventLog struct
 * @return 0 on success, -1 on error
 */
int logEventToServer(EventLog *event);

/**
 * @brief Periodic heartbeat to confirm ESP32 is online.
 *
 * GET /api/heartbeat.php
 *
 * @return 0 on success, -1 on error
 */
int sendHeartbeat();

#endif // COMM_H
