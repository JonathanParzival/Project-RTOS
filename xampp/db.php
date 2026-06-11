<?php
// htdocs/rfid/db.php
// Shared database connection — included by every API script

// Konfigurasi database XAMPP/MySQL lokal. Nilai default root tanpa password
// cocok untuk lab, tetapi wajib diganti saat dipakai di jaringan produksi.
define('DB_HOST', '127.0.0.1');
define('DB_PORT', 3306);
define('DB_NAME', 'rfid_attendance');
define('DB_USER', 'root');   // TODO: change in production
define('DB_PASS', '');       // TODO: set a password in production

/**
 * Membuat dan mengembalikan koneksi PDO singleton.
 *
 * Singleton static menjaga setiap request PHP hanya membuat satu koneksi DB.
 * Prepared statement dinonaktifkan dari emulasi agar parameter benar-benar
 * dikirim ke MySQL sebagai bind parameter.
 */
function getDB(): PDO {
    static $pdo = null;
    if ($pdo === null) {
        $dsn = sprintf('mysql:host=%s;port=%d;dbname=%s;charset=utf8mb4',
                       DB_HOST, DB_PORT, DB_NAME);
        $pdo = new PDO($dsn, DB_USER, DB_PASS, [
            PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            PDO::ATTR_EMULATE_PREPARES   => false,
        ]);
    }
    return $pdo;
}

// ── Helpers ───────────────────────────────────────────────────

/**
 * Mengirim response JSON dan menghentikan eksekusi script.
 *
 * Semua endpoint memakai helper ini agar status code, content-type, dan format
 * body konsisten untuk HTTPClient di ESP32.
 */
function jsonResponse(array $data, int $status = 200): void {
    http_response_code($status);
    header('Content-Type: application/json');
    echo json_encode($data);
    exit;
}

/**
 * Membaca body JSON mentah dari request.
 *
 * Jika body bukan JSON object/array valid, endpoint langsung mengembalikan 400
 * supaya firmware tidak mencoba memproses response yang ambigu.
 */
function getRequestBody(): array {
    $raw = file_get_contents('php://input');
    $data = json_decode($raw, true);
    if (!is_array($data)) {
        jsonResponse(['error' => 'Invalid JSON body'], 400);
    }
    return $data;
}

/**
 * Memastikan endpoint dipanggil dengan HTTP method yang benar.
 */
function requireMethod(string $method): void {
    if ($_SERVER['REQUEST_METHOD'] !== strtoupper($method)) {
        jsonResponse(['error' => "Method not allowed — use $method"], 405);
    }
}
