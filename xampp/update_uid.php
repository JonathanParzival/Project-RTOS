<?php
// htdocs/rfid/api/update_uid.php
// POST { "user_id": 3, "new_uid": "1234567A" }
// Response: { "success": true }

require_once __DIR__ . '/../db.php';

// Dipanggil setelah verify.php mengizinkan akses dan firmware menghitung token.
requireMethod('POST');
$body = getRequestBody();

// user_id berasal dari database, new_uid adalah rolling token 8 hex.
$user_id = (int)($body['user_id'] ?? 0);
$new_uid = strtoupper(trim($body['new_uid'] ?? ''));

if ($user_id <= 0) {
    jsonResponse(['error' => 'Missing or invalid user_id'], 400);
}

if (!preg_match('/^[0-9A-F]{8}$/', $new_uid)) {
    jsonResponse(['error' => 'Invalid new_uid format'], 400);
}

try {
    $db = getDB();

    // Make sure another user doesn't already own the new UID
    // (extremely unlikely with a proper rolling algorithm, but guard anyway)
    // Guard ini penting karena UID adalah kredensial; collision harus ditolak.
    $check = $db->prepare(
        'SELECT id FROM users WHERE uid = :uid AND id != :id LIMIT 1'
    );
    $check->execute([':uid' => $new_uid, ':id' => $user_id]);
    if ($check->fetch()) {
        jsonResponse(['error' => 'UID collision — new_uid already assigned to another user'], 409);
    }

    // Hanya user aktif yang boleh menerima token baru.
    $stmt = $db->prepare(
        'UPDATE users SET uid = :uid WHERE id = :id AND is_active = 1'
    );
    $stmt->execute([':uid' => $new_uid, ':id' => $user_id]);

    if ($stmt->rowCount() === 0) {
        jsonResponse(['error' => 'User not found or inactive'], 404);
    }

    // Response ringkas karena firmware hanya memerlukan status sukses/gagal.
    jsonResponse(['success' => true]);

} catch (PDOException $e) {
    error_log('[rfid/update_uid] DB error: ' . $e->getMessage());
    jsonResponse(['error' => 'Database error'], 500);
}
