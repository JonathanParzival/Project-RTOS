<?php
// htdocs/rfid/api/verify.php
// POST { "uid": "12345678" }
// Response: { "authorized": true/false, "user_id": int, "user_name": string }

require_once __DIR__ . '/../db.php';

// Firmware memanggil endpoint ini lewat HTTP POST dari AuthTask.
requireMethod('POST');
$body = getRequestBody();

// UID dinormalisasi uppercase agar perbandingan database tidak tergantung input.
$uid = strtoupper(trim($body['uid'] ?? ''));

// Basic format validation: exactly 8 hex characters
if (!preg_match('/^[0-9A-F]{8}$/', $uid)) {
    jsonResponse(['error' => 'Invalid UID format — must be 8 hex characters'], 400);
}

try {
    $db = getDB();

    // Prepared statement melindungi query dari SQL injection walaupun input sudah
    // divalidasi regex.
    $stmt = $db->prepare(
        'SELECT id, name FROM users
          WHERE uid = :uid AND is_active = 1
          LIMIT 1'
    );
    $stmt->execute([':uid' => $uid]);
    $row = $stmt->fetch();

    if ($row) {
        // Update last_seen sebagai audit kapan kartu valid terakhir discan.
        $upd = $db->prepare('UPDATE users SET last_seen = NOW() WHERE id = :id');
        $upd->execute([':id' => $row['id']]);

        // AuthTask memakai user_id untuk update_uid.php dan user_name untuk log.
        jsonResponse([
            'authorized' => true,
            'user_id'    => (int)$row['id'],
            'user_name'  => $row['name'],
        ]);
    } else {
        // UID tidak dikenal tetap response 200 agar firmware membedakan "akses
        // ditolak" dari "server/database error".
        jsonResponse([
            'authorized' => false,
            'user_id'    => 0,
            'user_name'  => '',
        ]);
    }

} catch (PDOException $e) {
    // Detail error hanya ditulis ke log server; ESP32 menerima pesan generik.
    error_log('[rfid/verify] DB error: ' . $e->getMessage());
    jsonResponse(['error' => 'Database error'], 500);
}
