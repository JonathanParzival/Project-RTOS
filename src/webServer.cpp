#include "webServer.h"
#include "config.h"
#include "tasks.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

AsyncWebServer server(WEB_SERVER_PORT);

int initWebServer() {
    server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request) {
        char *html_buffer = (char *)malloc(8192);
        if (html_buffer) {
            if (generateDashboardHTML(html_buffer, 8192, &uidDatabase) == 0) {
                request->send(200, "text/html", html_buffer);
            } else {
                request->send(500, "text/plain", "Error generating dashboard");
            }
            free(html_buffer); 
        } else {
            request->send(500, "text/plain", "Memory allocation failed");
        }
    });

    server.on("/api/database", HTTP_GET, [](AsyncWebServerRequest *request) {
        char *json_buffer = (char *)malloc(4096);
        if (json_buffer) {
            if (xSemaphoreTake(databaseMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                if (generateDatabaseJSON(json_buffer, 4096, &uidDatabase) == 0) {
                    request->send(200, "application/json", json_buffer);
                } else {
                    request->send(500, "application/json", "{\"error\":\"Buffer overflow\"}");
                }
                xSemaphoreGive(databaseMutex);
            } else {
                request->send(503, "application/json", "{\"error\":\"Database busy\"}");
            }
            free(json_buffer);
        } else {
            request->send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
        }
    });

    server.on("/api/register", HTTP_POST, [](AsyncWebServerRequest *request) {},
              nullptr,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (len > 0) {
            JsonDocument doc; 
            char json_str[256];
            memcpy(json_str, data, len);
            json_str[len] = '\0';

            DeserializationError error = deserializeJson(doc, json_str);
            if (!error) {
                const char* uid = doc["uid"];
                const char* name = doc["name"];

                if (uid && name && validateUIDFormat(uid) && validateNameFormat(name)) {
                    if (checkDuplicateUID(uid, &uidDatabase)) {
                        request->send(409, "application/json", "{\"error\":\"UID already exists\"}");
                        return;
                    }

                    HTTPRequest httpReq;
                    httpReq.type = HTTP_POST_REGISTER;
                    strncpy(httpReq.uid, uid, 8);
                    strncpy(httpReq.name, name, 49);

                    if (xQueueSend(httpRequestQueue, &httpReq, pdMS_TO_TICKS(100)) == pdPASS) {
                        request->send(200, "application/json", 
                                     "{\"status\":\"success\",\"message\":\"UID registered\"}");
                    } else {
                        request->send(503, "application/json", 
                                     "{\"error\":\"Request queue full\"}");
                    }
                } else {
                    request->send(400, "application/json", 
                                 "{\"error\":\"Invalid UID or name format\"}");
                }
            } else {
                request->send(400, "application/json", 
                             "{\"error\":\"Invalid JSON\"}");
            }
        }
    });

    server.on("^\\/api\\/uid\\/([a-fA-F0-9]{8})$", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        String uid = request->pathArg(0);
        
        if (validateUIDFormat(uid.c_str())) {
            HTTPRequest httpReq;
            httpReq.type = HTTP_DELETE_UID;
            strncpy(httpReq.uid, uid.c_str(), 8);

            if (xQueueSend(httpRequestQueue, &httpReq, pdMS_TO_TICKS(100)) == pdPASS) {
                request->send(200, "application/json", 
                             "{\"status\":\"success\",\"message\":\"UID deleted\"}");
            } else {
                request->send(503, "application/json", 
                             "{\"error\":\"Request queue full\"}");
            }
        } else {
            request->send(400, "application/json", 
                         "{\"error\":\"Invalid UID format\"}");
        }
    });

    server.onNotFound(handleNotFound);

    return 0;
}

int startWebServer() {
    server.begin();
    return 0;
}

int stopWebServer() {
    server.end();
    return 0;
}

int isWebServerRunning() {
    return 1;
}

void handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

int generateDashboardHTML(char *buffer, int buffer_size, UIDDatabase *db) {
    if (buffer == NULL || buffer_size < 2000) return -1;

    // Strict delimiter R"=====(...)=====" prevents JS quote issues in C++
    const char html_template[] = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RFID Attendance System</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial, sans-serif; background: #f5f5f5; padding: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        header { background: #2c3e50; color: white; padding: 20px; border-radius: 5px; margin-bottom: 20px; }
        .section { background: white; padding: 20px; margin-bottom: 20px; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        h2 { color: #2c3e50; margin-bottom: 15px; }
        input, button { padding: 10px; margin: 5px 0; width: 100%; border: 1px solid #ddd; border-radius: 3px; }
        button { background: #3498db; color: white; cursor: pointer; border: none; }
        button:hover { background: #2980b9; }
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background: #ecf0f1; }
        .delete-btn { background: #e74c3c; padding: 5px 10px; width: auto; }
        .delete-btn:hover { background: #c0392b; }
        .success { color: #27ae60; }
        .error { color: #e74c3c; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>RFID Attendance System</h1>
            <p>Web Management Dashboard</p>
        </header>

        <div class="section">
            <h2>Register New Card</h2>
            <div id="register-form">
                <input type="text" id="uid-input" placeholder="UID (8 hex chars: 12345678)" maxlength="8">
                <input type="text" id="name-input" placeholder="User Name">
                <button onclick="registerUID()">Register Card</button>
            </div>
            <p id="register-msg"></p>
        </div>

        <div class="section">
            <h2>Registered Cards</h2>
            <table id="uid-table">
                <thead>
                    <tr>
                        <th>UID</th>
                        <th>Name</th>
                        <th>Registered</th>
                        <th>Action</th>
                    </tr>
                </thead>
                <tbody id="uid-tbody">
                </tbody>
            </table>
        </div>
    </div>

    <script>
        function registerUID() {
            const uid = document.getElementById('uid-input').value.trim().toUpperCase();
            const name = document.getElementById('name-input').value.trim();
            const msgEl = document.getElementById('register-msg');

            if (!uid || !name) {
                msgEl.innerHTML = '<span class="error">Please fill all fields</span>';
                return;
            }

            if (uid.length !== 8) {
                msgEl.innerHTML = '<span class="error">UID must be 8 hex characters</span>';
                return;
            }

            fetch('/api/register', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({uid: uid, name: name})
            })
            .then(r => r.json())
            .then(data => {
                if (data.status === 'success') {
                    msgEl.innerHTML = '<span class="success">✓ Card registered!</span>';
                    document.getElementById('uid-input').value = '';
                    document.getElementById('name-input').value = '';
                    loadDatabase();
                } else {
                    msgEl.innerHTML = '<span class="error">✗ ' + (data.error || 'Registration failed') + '</span>';
                }
            })
            .catch(e => msgEl.innerHTML = '<span class="error">Error: ' + e + '</span>');
        }

        function deleteUID(uid) {
            if (!confirm('Delete UID: ' + uid + '?')) return;
            
            fetch('/api/uid/' + uid, {method: 'DELETE'})
            .then(r => r.json())
            .then(data => {
                if (data.status === 'success') {
                    loadDatabase();
                } else {
                    alert('Delete failed: ' + (data.error || 'Unknown error'));
                }
            });
        }

        function loadDatabase() {
            fetch('/api/database')
            .then(r => r.json())
            .then(data => {
                const tbody = document.getElementById('uid-tbody');
                tbody.innerHTML = '';
                if (data.entries) {
                    data.entries.forEach(entry => {
                        const row = `<tr>
                            <td>${entry.uid}</td>
                            <td>${entry.name}</td>
                            <td>${new Date(entry.timestamp_reg).toLocaleString()}</td>
                            <td><button class="delete-btn" onclick="deleteUID('${entry.uid}')">Delete</button></td>
                        </tr>`;
                        tbody.innerHTML += row;
                    });
                }
            });
        }

        window.onload = loadDatabase;
        setInterval(loadDatabase, 5000);
    </script>
</body>
</html>
    )=====";

    strncpy(buffer, html_template, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return 0;
}

int generateDatabaseJSON(char *buffer, int buffer_size, UIDDatabase *db) {
    if (buffer == NULL || db == NULL || buffer_size < 512) return -1;

    JsonDocument doc; // Updated for ArduinoJson v7
    JsonArray entries = doc["entries"].to<JsonArray>();

    for (int i = 0; i < db->count; i++) {
        JsonObject entry = entries.add<JsonObject>();
        entry["uid"] = db->entries[i].uid;
        entry["name"] = db->entries[i].name;
        entry["timestamp_reg"] = db->entries[i].timestamp_reg;
    }

    doc["count"] = db->count;
    doc["last_sync"] = db->last_sync;

    serializeJson(doc, buffer, buffer_size);
    return 0;
}

int validateUIDFormat(const char *uid_str) {
    if (uid_str == NULL) return 0;
    if (strlen(uid_str) != 8) return 0;

    for (int i = 0; i < 8; i++) {
        if (!isxdigit(uid_str[i])) return 0;
    }
    return 1;
}

int validateNameFormat(const char *name) {
    if (name == NULL) return 0;
    if (strlen(name) < 1 || strlen(name) > 49) return 0;
    return 1;
}

int checkDuplicateUID(const char *uid_str, UIDDatabase *db) {
    if (uid_str == NULL || db == NULL) return 0;

    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->entries[i].uid, uid_str) == 0) {
            return 1; 
        }
    }
    return 0;
}