#ifndef SECURITY_H
#define SECURITY_H

#include <stdint.h>
#include "data_structures.h"
#include "spiffs_manager.h"   // provides UIDDatabase and UIDEntry

// ============ ROLLING TOKEN ALGORITHM ============

/**
 * @brief Hitung UID berikutnya dengan rolling-token 12 bit.
 *
 * current_uid dan new_uid harus menunjuk ke buffer 4 byte. Prefix UID dijaga,
 * suffix dinaikkan sesuai ROLLING_TOKEN_STEP.
 */
int calculateRollingToken(uint8_t *current_uid, uint8_t *new_uid);

/**
 * @brief Cari UID binary pada database lokal SPIFFS.
 *
 * Return index entry bila cocok, atau -1 jika tidak ditemukan/error. out_entry
 * boleh NULL jika caller hanya butuh status cocok.
 */
int verifyUID(uint8_t *scanned_uid, UIDDatabase *db, UIDEntry *out_entry);

/**
 * @brief Simpan UID rolling-token baru pada entry database lokal.
 */
int updateUIDInDatabase(UIDDatabase *db, int uid_index, uint8_t *new_uid);

/**
 * @brief Konversi UID 4 byte ke string hex 8 karakter.
 *
 * hex_string harus minimal 9 byte termasuk terminator.
 */
int uidBinaryToHex(uint8_t *binary_uid, char *hex_string);

/**
 * @brief Konversi string hex 8 karakter ke UID 4 byte.
 */
int uidHexToBinary(const char *hex_string, uint8_t *binary_uid);

/**
 * @brief Bandingkan dua UID 4 byte; return 1 jika sama.
 */
int uidCompare(uint8_t *uid1, uint8_t *uid2);

/**
 * @brief Salin UID 4 byte dari src ke dst.
 */
void uidCopy(uint8_t *src, uint8_t *dst);

// ============ SECURITY STATE MANAGEMENT ============

/**
 * @brief Tambah counter gagal dan aktifkan lockout bila ambang tercapai.
 */
int recordFailedAttempt(SecurityState *state);

/**
 * @brief Return 1 bila sistem masih lockout; auto-unlock bila waktu habis.
 */
int isSystemLocked(SecurityState *state);

/**
 * @brief Reset counter gagal dan buka lockout.
 */
void clearFailedAttempts(SecurityState *state);

/**
 * @brief Kunci sistem sampai millis() + duration_ms.
 */
void lockSystem(SecurityState *state, uint32_t duration_ms);

// ============ HARDWARE FEEDBACK ============

/**
 * @brief Pola sukses: LED hijau dan beep pendek.
 */
void feedbackSuccess();

/**
 * @brief Pola gagal: LED merah dan double beep.
 */
void feedbackFailure();

/**
 * @brief Pola lockout: LED merah berkedip cepat dan beep panjang.
 */
void feedbackLockout();

#endif // SECURITY_H
