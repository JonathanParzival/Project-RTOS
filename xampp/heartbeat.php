<?php
// htdocs/rfid/api/heartbeat.php
// GET — returns 200 + server timestamp; upserts device row

require_once __DIR__ . '/../db.php';

// CommTask memanggil endpoint ini secara periodik dengan HTTP GET.
requireMethod('GET');

// REMOTE_ADDR membantu operator melihat IP ESP32 terbaru di tabel devices.
$ip = $_SERVER['REMOTE_ADDR'] ?? null;

try {
    $db = getDB();

    // Upsert device record
    // device_id dibuat tetap karena contoh proyek hanya memakai satu ESP32.
    $stmt = $db->prepare(
        'INSERT INTO devices (device_id, ip_address, last_ping)
              VALUES (:did, :ip, NOW())
         ON DUPLICATE KEY UPDATE ip_address = :ip, last_ping = NOW()'
    );
    $stmt->execute([':did' => 'esp32-rfid-01', ':ip' => $ip]);

} catch (PDOException $e) {
    // Heartbeat should never fail the ESP32 due to a DB hiccup
    // Karena itu error DB hanya dilog, lalu response tetap status ok.
    error_log('[rfid/heartbeat] DB error: ' . $e->getMessage());
}

// Timestamp server memudahkan pengecekan manual selisih waktu ESP32/backend.
jsonResponse([
    'status'     => 'ok',
    'server_time' => date('Y-m-d H:i:s'),
]);
