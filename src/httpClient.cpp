#include "comm.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>

// ============ WIFI ============

/**
 * @brief Menghubungkan ESP32 ke access point WiFi.
 *
 * Fungsi ini blocking dengan batas 20 percobaan x 500 ms. Karena dipanggil dari
 * setup atau CommTask, blocking singkat ini tidak dilakukan dari ISR.
 */
int initWiFi(const char *ssid, const char *password) {
    if (ssid == NULL || password == NULL) return -1;
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    return (WiFi.status() == WL_CONNECTED) ? 0 : -1;
}

/**
 * @brief Membaca status WiFi Arduino sebagai nilai integer sederhana.
 */
int isWiFiConnected() {
    return (WiFi.status() == WL_CONNECTED) ? 1 : 0;
}

/**
 * @brief Mengembalikan RSSI WiFi, atau -100 dBm sebagai nilai fallback putus.
 */
int getWiFiSignalStrength() {
    return (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -100;
}

/**
 * @brief Mengembalikan pointer ke string IP lokal ESP32.
 *
 * String dibuat static agar pointer tetap valid setelah fungsi return. Nilainya
 * akan berubah pada pemanggilan berikutnya, jadi jangan disimpan jangka panjang.
 */
const char* getESP32IPAddress() {
    static String ip_string;
    ip_string = WiFi.localIP().toString();
    return ip_string.c_str();
}

// ============ INTERNAL HELPER ============

/**
 * @brief Helper internal untuk HTTP POST JSON.
 *
 * Mengembalikan HTTP status code (>0) atau -1 bila WiFi/koneksi gagal.
 * response_body diisi sampai resp_len-1 byte agar selalu null-terminated.
 *
 * Catatan I/O dan memori:
 * - Buffer request/response disediakan caller supaya alokasi heap lebih terkontrol.
 * - HTTP timeout dari config.h mencegah task jaringan blocking terlalu lama.
 */
static int httpPostJSON(const char *url,
                        const char *json_body,
                        char       *response_body,
                        int         resp_len) {
    // Hindari membuat HTTPClient bila WiFi jelas belum tersambung.
    if (!isWiFiConnected()) return -1;

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);

    int code = http.POST((uint8_t *)json_body, strlen(json_body));

    if (code > 0 && response_body != NULL) {
        String resp = http.getString();
        // strncpy dibatasi oleh resp_len-1 dan terminator dipasang manual untuk
        // melindungi parser JSON dari buffer tidak berakhir null.
        strncpy(response_body, resp.c_str(), resp_len - 1);
        response_body[resp_len - 1] = '\0';
    }

    // Selalu akhiri sesi agar socket/buffer HTTPClient dilepas.
    http.end();
    return code;
}

// ============ API CALLS ============

/**
 * @brief Memverifikasi UID ke endpoint verify.php.
 *
 * Return 0 berarti request dan parsing JSON berhasil; hasil akses ada pada
 * response->authorized. Return -1 berarti masalah jaringan, status HTTP, atau
 * JSON sehingga AuthTask dapat membedakannya dari akses ditolak biasa.
 */
int verifyUIDWithServer(const char *uid_hex, ServerAuthResponse *response) {
    if (uid_hex == NULL || response == NULL) return -1;

    // Bangun JSON kecil di stack; isinya hanya UID 8 karakter.
    JsonDocument req_doc;
    req_doc["uid"] = uid_hex;
    char req_body[64];
    serializeJson(req_doc, req_body, sizeof(req_body));

    // Response 256 byte cukup untuk authorized/user_id/user_name sederhana.
    char resp_body[256] = {0};
    int http_code = httpPostJSON(API_VERIFY_UID, req_body, resp_body, sizeof(resp_body));

    if (http_code != 200) {
        // Reset output agar caller tidak membaca data lama saat error.
        Serial.printf("[HTTP] verifyUID failed, code=%d\n", http_code);
        response->authorized = 0;
        response->user_id = 0;
        response->user_name[0] = '\0';
        return -1;
    }

    // Parse response JSON dari PHP. Default operator | menjaga field hilang tetap
    // memiliki nilai aman.
    JsonDocument resp_doc;
    DeserializationError err = deserializeJson(resp_doc, resp_body);
    if (err) {
        Serial.println("[HTTP] verifyUID: JSON parse error");
        return -1;
    }

    response->authorized = resp_doc["authorized"] | false;
    response->user_id    = resp_doc["user_id"]    | 0;
    strlcpy(response->user_name,
            resp_doc["user_name"] | "",
            sizeof(response->user_name));

    return 0;
}

/**
 * @brief Mengirim UID rolling-token baru untuk user aktif.
 */
int updateUIDOnServer(int user_id, const char *new_uid_hex) {
    if (new_uid_hex == NULL) return -1;

    // Payload kecil: id user dari verify.php dan UID baru 8 hex.
    JsonDocument req_doc;
    req_doc["user_id"] = user_id;
    req_doc["new_uid"] = new_uid_hex;
    char req_body[128];
    serializeJson(req_doc, req_body, sizeof(req_body));

    int http_code = httpPostJSON(API_UPDATE_UID, req_body, NULL, 0);
    if (http_code != 200) {
        Serial.printf("[HTTP] updateUID failed, code=%d\n", http_code);
        return -1;
    }
    return 0;
}

/**
 * @brief Mengirim EventLog firmware ke endpoint log_event.php.
 *
 * Logging dipanggil best-effort oleh AuthTask; kegagalan tidak boleh mengubah
 * keputusan akses karena event utamanya sudah diproses lokal.
 */
int logEventToServer(EventLog *event) {
    if (event == NULL) return -1;

    // Konversi UID binary ke string karena PHP/MySQL menyimpan field UID teks.
    char uid_hex[9];
    snprintf(uid_hex, sizeof(uid_hex), "%02X%02X%02X%02X",
             event->uid[0], event->uid[1],
             event->uid[2], event->uid[3]);

    // Semua field EventLog disalin ke JSON; ukuran buffer 512 dipilih untuk
    // message hingga 100 byte dan user_name hingga 50 byte.
    JsonDocument req_doc;
    req_doc["type"]      = (int)event->type;
    req_doc["timestamp"] = event->timestamp;
    req_doc["uid"]       = uid_hex;
    req_doc["user_name"] = event->user_name;
    req_doc["result"]    = event->result;
    req_doc["message"]   = event->message;

    char req_body[512];
    serializeJson(req_doc, req_body, sizeof(req_body));

    int http_code = httpPostJSON(API_LOG_EVENT, req_body, NULL, 0);
    if (http_code != 200) {
        Serial.printf("[HTTP] logEvent failed, code=%d\n", http_code);
        return -1;
    }
    return 0;
}

/**
 * @brief Ping server XAMPP agar CommTask tahu backend masih responsif.
 */
int sendHeartbeat() {
    if (!isWiFiConnected()) return -1;

    HTTPClient http;
    http.begin(API_HEARTBEAT);
    http.setTimeout(HTTP_TIMEOUT_MS);
    int code = http.GET();
    http.end();

    return (code == 200) ? 0 : -1;
}
