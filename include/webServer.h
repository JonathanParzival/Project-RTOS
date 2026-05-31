#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>
#include "data_structures.h"

// Forward declaration needed for the correct callback signature
class AsyncWebServerRequest;

// ============ WEB SERVER INITIALIZATION ============

/**
 * @brief Initialize AsyncWebServer
 * Sets up HTTP routes and handlers
 * * @return 0 if success
 */
int initWebServer();

/**
 * @brief Start web server
 * * @return 0 if success
 */
int startWebServer();

/**
 * @brief Stop web server
 * * @return 0 if success
 */
int stopWebServer();

/**
 * @brief Check if web server is running
 * * @return 1 if running, 0 if stopped
 */
int isWebServerRunning();

// ============ HTTP ROUTE HANDLERS ============

void handleAdminDashboard();
void handleGetDatabase();
void handleRegisterUID();
void handleDeleteUID();
void handleResetSystem();

/**
 * @brief Handle 404 Not Found
 */
void handleNotFound(AsyncWebServerRequest *request);

// ============ HTML ASSET GENERATION ============

int generateDashboardHTML(char *buffer, int buffer_size, UIDDatabase *db);
int generateDatabaseJSON(char *buffer, int buffer_size, UIDDatabase *db);

// ============ REQUEST VALIDATION ============

int validateUIDFormat(const char *uid_str);
int validateNameFormat(const char *name);
int checkDuplicateUID(const char *uid_str, UIDDatabase *db);

#endif // WEB_SERVER_H