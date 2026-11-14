#pragma once

// ============================================================================
// 機密組態範本：請複製為 include/secrets.h 並填入真實設定
// 注意：include/secrets.h 已被 .gitignore 排除，嚴禁提交至版控！
// ============================================================================

// Wi-Fi 設定
#define WIFI_SSID     "REPLACE_WITH_YOUR_WIFI_SSID"
#define WIFI_PASSWORD "REPLACE_WITH_YOUR_WIFI_PASSWORD"

// OpenWeatherMap 2.5 API Key
#define OWM_API_KEY   "REPLACE_WITH_YOUR_OPENWEATHER_API_KEY"

// AI 額度資料來源端點（開發階段可指向本地 Mock Server，例如 http://192.168.1.100:8080/quota.json）
// 未來串接 stonez56.com 時改為 https://stonez56.com/api/quota
#define QUOTA_API_URL   "REPLACE_WITH_QUOTA_API_URL"
#define QUOTA_API_TOKEN "REPLACE_WITH_BEARER_TOKEN_OR_EMPTY"

// Fail-Closed 檢查：確保未替換佔位符時無法通過編譯
#if defined(ENABLE_NETWORK_SYNC)
  #if __has_include(<string.h>)
    // 編譯期字串佔位檢查可搭配執行期 Fail-Closed 自檢
  #endif
#endif

