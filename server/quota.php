<?php
declare(strict_types=1);

/**
 * CodexCredits Relay API (PHP 8.3)
 * 部署路徑範例：https://stonez56.com/api/quota.php
 */

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, no-cache, must-revalidate');

const MAX_PAYLOAD_BYTES = 4096;
const DATA_FILE = __DIR__ . '/quota_data.json';
const CONFIG_FILE = __DIR__ . '/config.php';

// 1. Fail-Closed 載入設定（環境變數優先，其次 config.php）
$expectedToken = getenv('QUOTA_API_TOKEN') ?: '';
if ($expectedToken === '' && file_exists(CONFIG_FILE)) {
    $cfg = require CONFIG_FILE;
    if (is_array($cfg) && isset($cfg['api_token'])) {
        $expectedToken = (string)$cfg['api_token'];
    }
}

if ($expectedToken === '' || str_starts_with($expectedToken, 'REPLACE_WITH_')) {
    http_response_code(500);
    echo json_encode(['error' => 'Server misconfigured: API token not set']);
    exit;
}

// 2. 取得並驗證 Bearer Token
function getBearerToken(): string {
    $auth = $_SERVER['HTTP_AUTHORIZATION'] ?? $_SERVER['REDIRECT_HTTP_AUTHORIZATION'] ?? '';
    if ($auth === '' && function_exists('getallheaders')) {
        $headers = getallheaders();
        foreach ($headers as $k => $v) {
            if (strtolower((string)$k) === 'authorization') {
                $auth = (string)$v;
                break;
            }
        }
    }
    if (str_starts_with($auth, 'Bearer ')) {
        return trim(substr($auth, 7));
    }
    return '';
}

$providedToken = getBearerToken();
if ($providedToken === '' || !hash_equals($expectedToken, $providedToken)) {
    http_response_code(401);
    echo json_encode(['error' => 'Unauthorized']);
    exit;
}

$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';

// 3. POST：接收本機 Pusher 上傳的最新額度 JSON
if ($method === 'POST') {
    $raw = file_get_contents('php://input', false, null, 0, MAX_PAYLOAD_BYTES + 1);
    if ($raw === false || $raw === '' || strlen($raw) > MAX_PAYLOAD_BYTES) {
        http_response_code(413);
        echo json_encode(['error' => 'Payload empty or exceeds 4096 bytes']);
        exit;
    }

    $data = json_decode($raw, true);
    if (!is_array($data) || !isset($data['codex_plus']) || !isset($data['gemini_pro_accounts'])) {
        http_response_code(400);
        echo json_encode(['error' => 'Invalid quota schema']);
        exit;
    }

    // 原子寫入避免讀取到寫到一半的檔案
    $tmpFile = DATA_FILE . '.tmp';
    $encoded = json_encode($data, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
    if ($encoded === false || file_put_contents($tmpFile, $encoded, LOCK_EX) === false) {
        http_response_code(500);
        echo json_encode(['error' => 'Failed to persist quota data']);
        exit;
    }
    rename($tmpFile, DATA_FILE);

    echo json_encode(['status' => 'ok', 'updated_at' => time()]);
    exit;
}

// 4. GET：提供 CrowPanel ESP32 拉取最新額度 JSON
if ($method === 'GET') {
    if (!file_exists(DATA_FILE)) {
        http_response_code(404);
        echo json_encode(['error' => 'No quota data uploaded yet']);
        exit;
    }
    readfile(DATA_FILE);
    exit;
}

http_response_code(405);
echo json_encode(['error' => 'Method not allowed']);

