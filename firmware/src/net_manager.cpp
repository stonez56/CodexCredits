#include "net_manager.h"
#include "config.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

#if __has_include("secrets.h")
  #include "secrets.h"
#endif

#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif
#ifndef OWM_API_KEY
  #define OWM_API_KEY ""
#endif
#ifndef QUOTA_API_URL
  #define QUOTA_API_URL ""
#endif
#ifndef QUOTA_API_TOKEN
  #define QUOTA_API_TOKEN ""
#endif

namespace {
  constexpr char NVS_NS[] = "codex_cfg";
  constexpr char AP_NAME[] = "CrowPanel-Setup";
  constexpr uint32_t HTTP_TIMEOUT_MS = 8000;
}

bool NetManager::isPlaceholder(const char* val) {
  if (!val || val[0] == '\0') return true;
  return strncmp(val, "REPLACE_WITH_", 13) == 0;
}

void NetManager::loadConfig(RuntimeConfig& cfg) {
  memset(&cfg, 0, sizeof(cfg));
  cfg.cityIndex = AppConfig::DEFAULT_CITY_INDEX;
  cfg.lang = LangMode::ZH_TW;
  cfg.autoCycleEnabled = true;

  Preferences prefs;
  if (prefs.begin(NVS_NS, true)) {
    String owm = prefs.getString("owm_key", "");
    String qUrl = prefs.getString("quota_url", "");
    String qTok = prefs.getString("quota_tok", "");
    cfg.cityIndex = prefs.getUChar("city_idx", AppConfig::DEFAULT_CITY_INDEX);
    cfg.lang = static_cast<LangMode>(prefs.getUChar("lang", static_cast<uint8_t>(LangMode::ZH_TW)));
    cfg.autoCycleEnabled = prefs.getBool("auto_cyc", true);

    strlcpy(cfg.owmApiKey, owm.c_str(), sizeof(cfg.owmApiKey));
    strlcpy(cfg.quotaUrl, qUrl.c_str(), sizeof(cfg.quotaUrl));
    strlcpy(cfg.quotaToken, qTok.c_str(), sizeof(cfg.quotaToken));
    prefs.end();
  }

  if (cfg.cityIndex >= AppConfig::CITY_COUNT) {
    cfg.cityIndex = AppConfig::DEFAULT_CITY_INDEX;
  }

  // 若 NVS 尚未設定，從 secrets.h 帶入非佔位符預設值
  if (isPlaceholder(cfg.owmApiKey) && !isPlaceholder(OWM_API_KEY)) {
    strlcpy(cfg.owmApiKey, OWM_API_KEY, sizeof(cfg.owmApiKey));
  }
  if (isPlaceholder(cfg.quotaUrl) && !isPlaceholder(QUOTA_API_URL)) {
    strlcpy(cfg.quotaUrl, QUOTA_API_URL, sizeof(cfg.quotaUrl));
  }
  if (isPlaceholder(cfg.quotaToken) && !isPlaceholder(QUOTA_API_TOKEN)) {
    strlcpy(cfg.quotaToken, QUOTA_API_TOKEN, sizeof(cfg.quotaToken));
  }
}

void NetManager::saveConfig(const RuntimeConfig& cfg) {
  Preferences prefs;
  if (prefs.begin(NVS_NS, false)) {
    prefs.putString("owm_key", cfg.owmApiKey);
    prefs.putString("quota_url", cfg.quotaUrl);
    prefs.putString("quota_tok", cfg.quotaToken);
    prefs.putUChar("city_idx", cfg.cityIndex);
    prefs.putUChar("lang", static_cast<uint8_t>(cfg.lang));
    prefs.putBool("auto_cyc", cfg.autoCycleEnabled);
    prefs.end();
  }
}

bool NetManager::begin(RuntimeConfig& cfg, PortalCallback onPortal) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("crowpanel-epaper");

  // 優先使用 WiFiManager 儲存在 NVS 的 Wi-Fi 憑證
  WiFi.begin();
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 8000) {
    delay(250);
  }

  // 若 NVS 無憑證或連線失敗，改試 secrets.h（若有效）
  if (WiFi.status() != WL_CONNECTED && !isPlaceholder(WIFI_SSID)) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 10000) {
      delay(250);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[NET] Wi-Fi connected, IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println("[NET] Wi-Fi connect failed, launching Config Portal...");
  return startConfigPortal(cfg, onPortal);
}

bool NetManager::startConfigPortal(RuntimeConfig& cfg, PortalCallback onPortal) {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);

  char cityBuf[4];
  snprintf(cityBuf, sizeof(cityBuf), "%u", cfg.cityIndex);

  WiFiManagerParameter pOwm("owm_key", "OpenWeather API Key", cfg.owmApiKey, sizeof(cfg.owmApiKey) - 1);
  WiFiManagerParameter pCity("city_idx", "City (0=Hsinchu,1=Taipei,2=Taichung,3=Kaohsiung)", cityBuf, 3);
  WiFiManagerParameter pUrl("quota_url", "Quota API URL (empty = Mock)", cfg.quotaUrl, sizeof(cfg.quotaUrl) - 1);
  WiFiManagerParameter pTok("quota_tok", "Quota Bearer Token (optional)", cfg.quotaToken, sizeof(cfg.quotaToken) - 1);

  wm.addParameter(&pOwm);
  wm.addParameter(&pCity);
  wm.addParameter(&pUrl);
  wm.addParameter(&pTok);

  wm.setAPCallback([&](WiFiManager* mgr) {
    Serial.printf("[NET] Config Portal AP: %s, IP: 192.168.4.1\n", AP_NAME);
    if (onPortal) {
      onPortal(AP_NAME, "192.168.4.1");
    }
  });

  bool ok = wm.startConfigPortal(AP_NAME);
  if (ok) {
    strlcpy(cfg.owmApiKey, pOwm.getValue(), sizeof(cfg.owmApiKey));
    strlcpy(cfg.quotaUrl, pUrl.getValue(), sizeof(cfg.quotaUrl));
    strlcpy(cfg.quotaToken, pTok.getValue(), sizeof(cfg.quotaToken));
    int cIdx = atoi(pCity.getValue());
    if (cIdx >= 0 && cIdx < static_cast<int>(AppConfig::CITY_COUNT)) {
      cfg.cityIndex = static_cast<uint8_t>(cIdx);
    }
    saveConfig(cfg);
    Serial.printf("[NET] Portal saved & connected, IP: %s\n", WiFi.localIP().toString().c_str());
  }
  return ok;
}

bool NetManager::syncNtp() {
  configTzTime(AppConfig::TZ_INFO, AppConfig::NTP_SERVER_1, AppConfig::NTP_SERVER_2);
  uint32_t start = millis();
  time_t now = time(nullptr);
  while (now < 1700000000 && (millis() - start) < 5000) {
    delay(200);
    now = time(nullptr);
  }
  if (now >= 1700000000) {
    Serial.printf("[NTP] Synced epoch: %ld\n", static_cast<long>(now));
    return true;
  }
  Serial.println("[NTP] Sync timeout");
  return false;
}

time_t NetManager::nowEpoch() const {
  return time(nullptr);
}

bool NetManager::isWifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

int8_t NetManager::wifiRssi() const {
  return isWifiConnected() ? static_cast<int8_t>(WiFi.RSSI()) : -127;
}

void NetManager::beginOTA() {
  if (otaStarted_ || !isWifiConnected()) return;
  ArduinoOTA.setHostname("crowpanel-epaper");
  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Update starting...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update complete, rebooting...");
  });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("[OTA] Error[%u]\n", err);
  });
  ArduinoOTA.begin();
  otaStarted_ = true;
  Serial.println("[OTA] Ready at crowpanel-epaper.local");
}

void NetManager::handleOTA() {
  if (otaStarted_) {
    ArduinoOTA.handle();
  }
}

bool NetManager::httpGet(const char* url, const char* bearerToken, String& outPayload, size_t maxBytes) {
  if (!url || url[0] == '\0' || WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  bool isHttps = strncmp(url, "https://", 8) == 0;
  bool began = false;

  if (isHttps) {
    secureClient.setInsecure();
    began = http.begin(secureClient, url);
  } else {
    began = http.begin(plainClient, url);
  }

  if (!began) return false;

  if (bearerToken && bearerToken[0] != '\0' && !isPlaceholder(bearerToken)) {
    String auth = "Bearer ";
    auth += bearerToken;
    http.addHeader("Authorization", auth);
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[HTTP] GET failed, code=%d\n", code);
    http.end();
    return false;
  }

  int len = http.getSize();
  if (len > 0 && static_cast<size_t>(len) > maxBytes) {
    Serial.printf("[HTTP] Payload too large: %d > %u\n", len, static_cast<unsigned>(maxBytes));
    http.end();
    return false;
  }

  outPayload = http.getString();
  http.end();

  if (outPayload.length() > maxBytes) {
    outPayload = "";
    return false;
  }
  return outPayload.length() > 0;
}

