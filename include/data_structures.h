#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <stdint.h>

// ============ RFID DATA MESSAGE (for queue) ============
// Payload kecil yang dikirim InputTask ke AuthTask lewat rfidDataQueue.
// Queue menyimpan salinan struct ini agar tidak bergantung pada lifetime stack.
typedef struct {
    uint8_t uid[4];           // 4-byte UID from reader
    uint32_t timestamp;       // millis() when detected
} RFIDData;

// ============ EVENT LOG TYPES ============
// Nilai enum ini dikirim ke LCD dan backend PHP. Jika urutan diubah, pastikan
// database/backend yang menyimpan event_type ikut disesuaikan.
typedef enum {
    EVENT_ACCESS_GRANTED    = 0,
    EVENT_ACCESS_DENIED     = 1,
    EVENT_SPOOFING_DETECTED = 2,
    EVENT_SYSTEM_LOCKOUT    = 3,
    EVENT_UID_REGISTERED    = 4,
    EVENT_DATABASE_SYNCED   = 5,
    EVENT_SERVER_ERROR      = 6,
    EVENT_ERROR             = 7
} EventType;

// ============ EVENT LOG STRUCTURE ============
// Used internally to drive the LCD display task
// Struct ini juga dipakai sebagai body log ke server. Ukuran string dibatasi agar
// payload queue tetap deterministik dan tidak memakai alokasi dinamis.
typedef struct {
    EventType type;
    uint32_t  timestamp;
    uint8_t   uid[4];
    char      user_name[50];
    uint8_t   result;
    char      message[100];
} EventLog;

// ============ SECURITY STATE STRUCTURE ============
// State lockout bersama AuthTask/SecurityTask. Field lockout_until memakai waktu
// absolut millis(), bukan durasi relatif.
typedef struct {
    int      failed_attempts;
    uint32_t lockout_until;
    uint32_t last_attempt;
    uint8_t  is_locked;
} SecurityState;

// ============ SERVER AUTH RESPONSE ============
// Populated by verifyUIDWithServer()
// Response sementara dari HTTP verify.php. AuthTask membaca authorized untuk
// keputusan akses dan user_id untuk update rolling token.
typedef struct {
    int  user_id;             // MySQL row id (used for update calls)
    char user_name[50];       // Human-readable name from DB
    int  authorized;          // 1 = access granted, 0 = denied / not found
} ServerAuthResponse;

#endif // DATA_STRUCTURES_H
