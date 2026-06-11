#include "config.h"
#include "spiffs_manager.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <stdio.h>

/**
 * @brief Mount filesystem SPIFFS.
 *
 * Parameter true membuat SPIFFS diformat bila mount gagal. Ini memudahkan
 * recovery perangkat, tetapi di produksi perlu dipertimbangkan karena data bisa
 * hilang bila partisi rusak.
 */
int initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        return -1;
    }
    return 0;
}

/**
 * @brief Mengecek apakah SPIFFS dapat di-mount.
 */
int isSPIFFSMounted() {
    return SPIFFS.begin(true);
}

/**
 * @brief Load UIDDatabase dari file JSON di SPIFFS.
 *
 * Fungsi ini mengisi struct caller secara langsung. Bila file belum ada, database
 * dianggap kosong dan tetap success agar first boot tidak gagal.
 */
int loadUIDsFromFile(UIDDatabase *db, const char *filepath) {
    if (db == NULL || filepath == NULL) return -1;

    if (!SPIFFS.exists(filepath)) {
        // First boot/default: database kosong dengan metadata dasar.
        db->count = 0;
        db->last_sync = millis();
        strcpy(db->version, "1.0");
        return 0;
    }

    File file = SPIFFS.open(filepath, "r");
    if (!file) return -1;

    // ArduinoJson membaca langsung dari stream File sehingga tidak perlu buffer
    // string file penuh di RAM.
    JsonDocument doc; // Updated for ArduinoJson v7
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) return -1;

    db->count = 0;
    if (doc["entries"].is<JsonArray>()) {
        JsonArray entries = doc["entries"];
        for (JsonObject entry : entries) {
            // Batasi jumlah entry sesuai kapasitas array statis UIDDatabase.
            if (db->count >= MAX_UID_ENTRIES) break;

            // strlcpy mencegah overflow pada field fixed-size.
            strlcpy(db->entries[db->count].uid, entry["uid"] | "", 9);
            strlcpy(db->entries[db->count].name, entry["name"] | "", 50);
            db->entries[db->count].timestamp_reg = entry["timestamp_reg"] | 0;
            db->entries[db->count].rolling_state = entry["rolling_state"] | 0;
            db->count++;
        }
    }

    db->last_sync = doc["last_sync"] | millis();
    strlcpy(db->version, doc["version"] | "1.0", 16);

    return 0;
}

/**
 * @brief Menyimpan UIDDatabase ke JSON di SPIFFS.
 */
int saveUIDsToFile(UIDDatabase *db, const char *filepath) {
    if (db == NULL || filepath == NULL) return -1;

    // Bangun dokumen JSON dari array statis database lokal.
    JsonDocument doc; // Updated for ArduinoJson v7
    JsonArray entries = doc["entries"].to<JsonArray>();
    
    for (int i = 0; i < db->count; i++) {
        JsonObject entry = entries.add<JsonObject>();
        entry["uid"] = db->entries[i].uid;
        entry["name"] = db->entries[i].name;
        entry["timestamp_reg"] = db->entries[i].timestamp_reg;
        entry["rolling_state"] = db->entries[i].rolling_state;
    }

    doc["count"] = db->count;
    doc["last_sync"] = db->last_sync;
    doc["version"] = db->version;

    File file = SPIFFS.open(filepath, "w");
    if (!file) return -1;

    // Serialize langsung ke File untuk mengurangi buffer RAM tambahan.
    serializeJson(doc, file);
    file.close();

    return 0;
}

/**
 * @brief Menambahkan satu UIDEntry ke file database lokal.
 *
 * UIDDatabase dialokasikan sementara di heap karena struct berisi array entry
 * cukup besar untuk stack task kecil.
 */
int addUIDToFile(UIDEntry *entry, const char *filepath) {
    if (entry == NULL || filepath == NULL) return -1;

    UIDDatabase *db = new UIDDatabase();
    if (db == NULL) return -1;

    if (loadUIDsFromFile(db, filepath) != 0) {
        delete db;
        return -1;
    }

    for (int i = 0; i < db->count; i++) {
        // Cegah duplicate UID agar lookup tidak ambigu.
        if (strcmp(db->entries[i].uid, entry->uid) == 0) {
            delete db;
            return -1;
        }
    }

    if (db->count >= MAX_UID_ENTRIES) {
        delete db;
        return -1;
    }

    memcpy(&db->entries[db->count], entry, sizeof(UIDEntry));
    db->count++;
    db->last_sync = millis();

    int result = saveUIDsToFile(db, filepath);
    delete db;
    return result;
}

/**
 * @brief Menghapus UID dari file database lokal.
 */
int deleteUIDFromFile(const char *uid_hex, const char *filepath) {
    if (uid_hex == NULL || filepath == NULL) return -1;

    UIDDatabase *db = new UIDDatabase();
    if (db == NULL) return -1;

    if (loadUIDsFromFile(db, filepath) != 0) {
        delete db;
        return -1;
    }

    int found_index = -1;
    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->entries[i].uid, uid_hex) == 0) {
            found_index = i;
            break;
        }
    }

    if (found_index == -1) {
        delete db;
        return -1;
    }

    // Geser elemen setelah index yang dihapus untuk menjaga array tetap rapat.
    for (int i = found_index; i < db->count - 1; i++) {
        memcpy(&db->entries[i], &db->entries[i + 1], sizeof(UIDEntry));
    }
    db->count--;
    db->last_sync = millis();

    int result = saveUIDsToFile(db, filepath);
    delete db;
    return result;
}

/**
 * @brief Cek keberadaan UID di file tanpa mengubah database.
 */
int uidExistsInFile(const char *uid_hex, const char *filepath) {
    if (uid_hex == NULL || filepath == NULL) return 0;

    UIDDatabase *db = new UIDDatabase();
    if (db == NULL) return 0;

    if (loadUIDsFromFile(db, filepath) != 0) {
        delete db;
        return 0;
    }

    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->entries[i].uid, uid_hex) == 0) {
            delete db;
            return 1;
        }
    }

    delete db;
    return 0;
}

/**
 * @brief Wrapper sederhana SPIFFS.exists() dengan validasi NULL.
 */
int fileExists(const char *filepath) {
    if (filepath == NULL) return 0;
    return SPIFFS.exists(filepath) ? 1 : 0;
}

/**
 * @brief Menghapus file SPIFFS.
 */
int deleteFile(const char *filepath) {
    if (filepath == NULL) return -1;
    return SPIFFS.remove(filepath) ? 0 : -1;
}

/**
 * @brief Mengambil ukuran file SPIFFS dalam byte.
 */
int getFileSize(const char *filepath) {
    if (filepath == NULL) return -1;
    if (!SPIFFS.exists(filepath)) return -1;

    File file = SPIFFS.open(filepath, "r");
    if (!file) return -1;

    int size = file.size();
    file.close();
    return size;
}

/**
 * @brief Mencetak daftar file SPIFFS ke Serial untuk debugging storage.
 */
void listSPIFFSFiles() {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();

    Serial.println("SPIFFS Files:");
    while (file) {
        Serial.printf("  %s - %d bytes\n", file.name(), file.size());
        file = root.openNextFile();
    }
}

/**
 * @brief Mengambil kapasitas total dan pemakaian SPIFFS.
 */
int getSPIFFSInfo(uint32_t *total_bytes, uint32_t *used_bytes) {
    if (total_bytes == NULL || used_bytes == NULL) return -1;
    *total_bytes = SPIFFS.totalBytes();
    *used_bytes = SPIFFS.usedBytes();
    return 0;
}
