<?php
declare(strict_types=1);

/**
 * CrowPanel ESP32-S3 TW & US Stock Relay API (PHP 8.3)
 *
 * Security:
 * - Fail-Closed: requires valid config.php with non-empty api_token
 * - Bearer Token checked via hash_equals()
 * - Server-side 5-minute file cache (stocks_cache.json) to minimize upstream calls
 */

header('Content-Type: application/json; charset=utf-8');
header('X-Content-Type-Options: nosniff');

$parentConfig = dirname(__DIR__) . '/config.php';
$localConfig  = __DIR__ . '/config.php';
$useParent    = is_file($parentConfig);
$configPath   = $useParent ? $parentConfig : $localConfig;

if (!is_file($configPath)) {
    http_response_code(503);
    echo json_encode(['error' => 'Server not configured']);
    exit;
}

$config = require $configPath;
$expectedToken = is_array($config) ? (string)($config['api_token'] ?? '') : '';
if ($expectedToken === '' || $expectedToken === 'YOUR_SUPER_SECRET_TOKEN_HERE') {
    http_response_code(503);
    echo json_encode(['error' => 'Invalid server token configuration']);
    exit;
}

$authHeader = $_SERVER['HTTP_AUTHORIZATION'] ?? $_SERVER['REDIRECT_HTTP_AUTHORIZATION'] ?? '';
if (!preg_match('/^Bearer\s+(\S+)$/i', trim($authHeader), $m)) {
    http_response_code(401);
    echo json_encode(['error' => 'Missing Bearer token']);
    exit;
}

if (!hash_equals($expectedToken, $m[1])) {
    http_response_code(403);
    echo json_encode(['error' => 'Forbidden']);
    exit;
}

function isAnyMarketOpen(): bool {
    $tz = new DateTimeZone('Asia/Taipei');
    $now = new DateTimeImmutable('now', $tz);
    $wday = (int)$now->format('N'); // 1=Mon .. 7=Sun
    $hhmm = (int)$now->format('Hi');

    // TW market: Mon-Fri 08:55 ~ 13:35 (includes post-close settlement buffer)
    if ($wday >= 1 && $wday <= 5 && $hhmm >= 855 && $hhmm <= 1335) {
        return true;
    }
    // US market: Mon-Fri 21:25 ~ 23:59, Tue-Sat 00:00 ~ 05:05 (covers DST & standard time)
    if ($wday >= 1 && $wday <= 5 && $hhmm >= 2125) {
        return true;
    }
    if ($wday >= 2 && $wday <= 6 && $hhmm <= 505) {
        return true;
    }
    return false;
}

const CACHE_TTL_OPEN_SEC = 300;    // 5 min during trading hours
const CACHE_TTL_CLOSED_SEC = 21600; // 6 hr off-hours: serve cache without hitting upstream APIs
$cacheFile = $useParent
    ? (dirname(__DIR__) . '/stocks_cache.json')
    : (__DIR__ . '/stocks_cache.json');
$force = isset($_GET['force']) && $_GET['force'] === '1';
$ttl = isAnyMarketOpen() ? CACHE_TTL_OPEN_SEC : CACHE_TTL_CLOSED_SEC;

if (!$force && is_file($cacheFile)) {
    $mtime = filemtime($cacheFile);
    if ($mtime !== false && (time() - $mtime) < $ttl) {
        $cached = file_get_contents($cacheFile);
        if ($cached !== false && $cached !== '') {
            echo $cached;
            exit;
        }
    }
}

$SYMBOLS = [
    ['code' => '0050',   'twse' => 'tse_0050.tw',   'yahoo' => '0050.TW',   'zh' => '元大台灣50',      'en' => '0050 Yuanta50',  'dec' => 2],
    ['code' => '006208', 'twse' => 'tse_006208.tw', 'yahoo' => '006208.TW', 'zh' => '富邦台50',        'en' => '006208 Fubon50', 'dec' => 2],
    ['code' => '0056',   'twse' => 'tse_0056.tw',   'yahoo' => '0056.TW',   'zh' => '元大高股息',      'en' => '0056 Div',       'dec' => 2],
    ['code' => '00878',  'twse' => 'tse_00878.tw',  'yahoo' => '00878.TW',  'zh' => '國泰永續高股息',  'en' => '00878 ESG Div',  'dec' => 2],
    ['code' => '00919',  'twse' => 'tse_00919.tw',  'yahoo' => '00919.TW',  'zh' => '群益台灣精選高息','en' => '00919 HiDiv',    'dec' => 2],
    ['code' => '2330',   'twse' => 'tse_2330.tw',   'yahoo' => '2330.TW',   'zh' => '台積電',          'en' => '2330 TSMC',      'dec' => 1],
    ['code' => '00937B', 'twse' => 'otc_00937B.tw', 'yahoo' => '00937B.TWO','zh' => '群益ESG投等債20+','en' => '00937B Bond',    'dec' => 2],
    ['code' => '00719B', 'twse' => 'otc_00719B.tw', 'yahoo' => '00719B.TWO','zh' => '元大美債1-3',     'en' => '00719B UST1-3',  'dec' => 2],
    ['code' => 'TWII',   'twse' => 'tse_t00.tw',    'yahoo' => '^TWII',     'zh' => '加權指數',        'en' => 'Tai-Index',      'dec' => 0],
    ['code' => 'GSPC',   'twse' => '',              'yahoo' => '^GSPC',     'zh' => '標普500',         'en' => 'US S&P 500',     'dec' => 0],
];

function httpGetJson(string $url, array $headers = []): ?array {
    $ch = curl_init($url);
    if ($ch === false) {
        return null;
    }
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT        => 8,
        CURLOPT_CONNECTTIMEOUT => 4,
        CURLOPT_FOLLOWLOCATION => true,
        CURLOPT_HTTPHEADER     => array_merge([
            'User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)',
        ], $headers),
    ]);
    $raw = curl_exec($ch);
    $code = (int)curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    if ($raw === false || $code < 200 || $code >= 300) {
        return null;
    }
    $decoded = json_decode($raw, true);
    return is_array($decoded) ? $decoded : null;
}

function fetchTwseMap(array $symbols): array {
    $exCh = [];
    foreach ($symbols as $s) {
        if ($s['twse'] !== '') {
            $exCh[] = $s['twse'];
        }
    }
    if ($exCh === []) {
        return [];
    }
    $url = 'https://mis.twse.com.tw/stock/api/getStockInfo.jsp?ex_ch='
        . urlencode(implode('|', $exCh)) . '&json=1&delay=0';
    $data = httpGetJson($url, ['Referer: https://mis.twse.com.tw/stock/fibest.jsp']);
    if ($data === null || empty($data['msgArray']) || !is_array($data['msgArray'])) {
        return [];
    }
    $map = [];
    foreach ($data['msgArray'] as $row) {
        $c = (string)($row['c'] ?? '');
        if ($c === '') {
            continue;
        }
        $z = (string)($row['z'] ?? '-');
        $y = (float)($row['y'] ?? 0);
        $price = ($z !== '-' && $z !== '' && is_numeric($z)) ? (float)$z : $y;
        if ($price <= 0) {
            continue;
        }
        $pct = ($y > 0) ? (($price - $y) / $y) * 100.0 : 0.0;
        $key = ($c === 't00') ? 'TWII' : $c;
        $map[$key] = [
            'price' => $price,
            'pct'   => round($pct, 2),
        ];
    }
    return $map;
}

function fetchYahooQuote(string $yahooSymbol): ?array {
    $url = 'https://query1.finance.yahoo.com/v8/finance/chart/'
        . rawurlencode($yahooSymbol) . '?interval=1d&range=2d';
    $data = httpGetJson($url);
    $meta = $data['chart']['result'][0]['meta'] ?? null;
    if (!is_array($meta)) {
        return null;
    }
    $price = (float)($meta['regularMarketPrice'] ?? 0);
    $prev  = (float)($meta['chartPreviousClose'] ?? ($meta['previousClose'] ?? 0));
    if ($price <= 0) {
        return null;
    }
    $pct = ($prev > 0) ? (($price - $prev) / $prev) * 100.0 : 0.0;
    return [
        'price' => $price,
        'pct'   => round($pct, 2),
    ];
}

$twseMap = fetchTwseMap($SYMBOLS);
$items = [];
foreach ($SYMBOLS as $s) {
    $code = $s['code'];
    $quote = $twseMap[$code] ?? null;
    if ($quote === null) {
        $quote = fetchYahooQuote($s['yahoo']);
    }
    $items[] = [
        'code'  => $code,
        'zh'    => $s['zh'],
        'en'    => $s['en'],
        'dec'   => $s['dec'],
        'valid' => $quote !== null,
        'price' => $quote !== null ? round($quote['price'], $s['dec']) : 0.0,
        'pct'   => $quote !== null ? $quote['pct'] : 0.0,
    ];
}

$payload = json_encode([
    'schema_version' => '1.0',
    'updated_epoch'  => time(),
    'items'          => $items,
], JSON_UNESCAPED_UNICODE);

if ($payload !== false) {
    $tmp = $cacheFile . '.tmp';
    if (file_put_contents($tmp, $payload, LOCK_EX) !== false) {
        @rename($tmp, $cacheFile);
    }
    echo $payload;
} else {
    http_response_code(500);
    echo json_encode(['error' => 'Encoding failed']);
}

