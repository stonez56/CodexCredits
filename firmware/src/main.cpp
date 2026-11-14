#include <Arduino.h>
#include "config.h"
#include "app_types.h"
#include "display_driver.h"
#include "input_handler.h"
#include "net_manager.h"
#include "quota_client.h"
#include "stock_client.h"
#include "weather_client.h"
#include "ui_renderer.h"

DisplayDriver g_display;
InputHandler  g_input;
NetManager    g_net;
QuotaClient   g_quotaClient;
StockClient   g_stockClient;
WeatherClient g_weatherClient;
UiRenderer    g_ui;

RuntimeConfig g_cfg;
QuotaState    g_quota;
StockState    g_stocks;
WeatherState  g_weather;

PageId   g_currentPage      = PageId::OVERVIEW;
uint32_t g_lastCycleMs      = 0;
uint32_t g_lastClockMs      = 0;
uint32_t g_lastQuotaMs      = 0;
uint32_t g_lastStockMs      = 0;
uint32_t g_lastWeatherMs    = 0;
uint32_t g_manualHoldUntil  = 0;
uint32_t g_nightWakeUntil   = 0;
bool     g_wasNightSleep    = false;

namespace {
  bool isNightHour(time_t nowEpoch) {
    if (nowEpoch < 1700000000) return false;
    struct tm t;
    localtime_r(&nowEpoch, &t);
    return (t.tm_hour >= AppConfig::NIGHT_START_HOUR && t.tm_hour < AppConfig::NIGHT_END_HOUR);
  }

  bool isMarketOpen(time_t nowEpoch) {
    if (nowEpoch < 1700000000) return false;
    struct tm t;
    localtime_r(&nowEpoch, &t);
    const int w = t.tm_wday; // 0=Sun .. 6=Sat
    const int hhmm = t.tm_hour * 100 + t.tm_min;
    if (w >= 1 && w <= 5 && hhmm >= 855 && hhmm <= 1335) return true;
    if (w >= 1 && w <= 5 && hhmm >= 2125) return true;
    if (w >= 2 && w <= 6 && hhmm <= 505) return true;
    return false;
  }

  void renderCurrent(bool forceFull) {
    const time_t nowEp = g_net.nowEpoch();
    const uint32_t nowMs = millis();
    const bool nightNow = isNightHour(nowEp);
    const bool nightSleep = nightNow && (static_cast<int32_t>(g_nightWakeUntil - nowMs) <= 0);
    const bool manualHold = (!nightSleep) && (static_cast<int32_t>(g_manualHoldUntil - nowMs) > 0);

    UiContext ctx {
      g_currentPage,
      g_cfg.lang,
      g_cfg.cityIndex,
      nowEp,
      g_net.isWifiConnected(),
      nightSleep,
      g_cfg.autoCycleEnabled,
      manualHold,
      &g_quota,
      &g_weather,
      &g_stocks
    };
    g_ui.render(g_display, ctx, forceFull);
  }

  void syncAllData(bool includeWeather, bool includeStocks = true) {
    const time_t nowEp = g_net.nowEpoch();
    g_quotaClient.sync(g_cfg.quotaUrl, g_cfg.quotaToken, nowEp, g_quota);
    if (includeStocks) {
      g_stockClient.sync(g_cfg.quotaUrl, g_cfg.quotaToken, nowEp, g_stocks);
    }
    if (includeWeather) {
      const char* cityQ = AppConfig::CITIES[g_cfg.cityIndex % AppConfig::CITY_COUNT].query;
      g_weatherClient.sync(g_cfg.owmApiKey, cityQ, nowEp, g_weather);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("\n=== CodexCredits CrowPanel 5.79\" Dashboard ===");

  memset(&g_quota, 0, sizeof(g_quota));
  memset(&g_stocks, 0, sizeof(g_stocks));
  memset(&g_weather, 0, sizeof(g_weather));

  g_input.begin();
  g_display.begin();
  g_ui.begin(g_display);
  g_ui.renderSplashScreen(g_display, AppConfig::APP_VERSION, "Connecting Wi-Fi & Syncing Data...");

  g_net.loadConfig(g_cfg);

  auto portalCb = [](const char* apName, const char* apIp) {
    g_ui.renderPortalScreen(g_display, apName, apIp);
  };

  if (g_net.begin(g_cfg, portalCb)) {
    g_net.syncNtp();
    g_net.beginOTA([]() {
      g_ui.renderSplashScreen(g_display, AppConfig::APP_VERSION, "OTA Updating... Do not power off");
    });
  }

  syncAllData(true);

  const uint32_t nowMs = millis();
  g_lastCycleMs   = nowMs;
  g_lastClockMs   = nowMs;
  g_lastQuotaMs   = nowMs;
  g_lastStockMs   = nowMs;
  g_lastWeatherMs = nowMs;

  if (isNightHour(g_net.nowEpoch())) {
    g_currentPage = PageId::OVERVIEW;
    g_wasNightSleep = true;
  }

  renderCurrent(true);
}

void loop() {
  g_net.handleOTA();

  const uint32_t nowMs = millis();
  const time_t nowEp   = g_net.nowEpoch();
  const bool nightNow  = isNightHour(nowEp);

  // 1. 處理實體按鍵事件
  InputEvent ev = g_input.poll();
  if (ev != InputEvent::NONE) {
    Serial.printf("[INPUT] %s\n", InputHandler::eventName(ev));
    if (nightNow) {
      g_nightWakeUntil = nowMs + AppConfig::NIGHT_WAKE_HOLD_SEC * 1000UL;
    }

    bool needRender = false;
    bool forceFull  = false;

    switch (ev) {
      case InputEvent::UP_SHORT:
        g_currentPage = static_cast<PageId>((static_cast<uint8_t>(g_currentPage) + static_cast<uint8_t>(PageId::PAGE_COUNT) - 1) % static_cast<uint8_t>(PageId::PAGE_COUNT));
        g_manualHoldUntil = nowMs + AppConfig::MANUAL_HOLD_SEC * 1000UL;
        needRender = true;
        break;

      case InputEvent::DOWN_SHORT:
        g_currentPage = static_cast<PageId>((static_cast<uint8_t>(g_currentPage) + 1) % static_cast<uint8_t>(PageId::PAGE_COUNT));
        g_manualHoldUntil = nowMs + AppConfig::MANUAL_HOLD_SEC * 1000UL;
        needRender = true;
        break;

      case InputEvent::CLICK_SHORT:
        g_cfg.lang = (g_cfg.lang == LangMode::ZH_TW) ? LangMode::EN : LangMode::ZH_TW;
        g_net.saveConfig(g_cfg);
        Serial.printf("[UI] Language switched to: %s\n", g_cfg.lang == LangMode::ZH_TW ? "ZH_TW" : "EN");
        needRender = true;
        forceFull  = true;
        break;

      case InputEvent::CLICK_LONG:
        g_cfg.autoCycleEnabled = !g_cfg.autoCycleEnabled;
        g_net.saveConfig(g_cfg);
        g_manualHoldUntil = 0;
        Serial.printf("[UI] Auto-cycle: %s\n", g_cfg.autoCycleEnabled ? "AUTO" : "LOCK");
        needRender = true;
        break;

      case InputEvent::MENU_SHORT:
      case InputEvent::MENU_LONG:
        if (g_currentPage == PageId::WEATHER_5DAY) {
          g_cfg.cityIndex = (g_cfg.cityIndex + 1) % AppConfig::CITY_COUNT;
          g_net.saveConfig(g_cfg);
          syncAllData(true);
        } else {
          syncAllData(false);
        }
        g_manualHoldUntil = nowMs + AppConfig::MANUAL_HOLD_SEC * 1000UL;
        needRender = true;
        forceFull  = true;
        break;

      case InputEvent::EXIT_SHORT:
        g_currentPage = PageId::OVERVIEW;
        g_cfg.autoCycleEnabled = true;
        g_net.saveConfig(g_cfg);
        g_manualHoldUntil = 0;
        g_lastCycleMs = nowMs;
        needRender = true;
        break;

      default:
        break;
    }

    if (needRender) {
      renderCurrent(forceFull);
      g_lastClockMs = nowMs;
      g_lastCycleMs = nowMs;
    }
  }

  // 2. 夜間模式 (00:00 ~ 07:00) 狀態管理
  const bool nightSleep = nightNow && (static_cast<int32_t>(g_nightWakeUntil - nowMs) <= 0);
  if (nightSleep) {
    if (!g_wasNightSleep || g_currentPage != PageId::OVERVIEW) {
      g_wasNightSleep = true;
      g_currentPage = PageId::OVERVIEW;
      renderCurrent(true);
      g_lastClockMs = nowMs;
    }
    if ((nowMs - g_lastWeatherMs) >= AppConfig::NIGHT_SYNC_INTERVAL_MS) {
      syncAllData(true);
      g_lastQuotaMs = nowMs;
      g_lastWeatherMs = nowMs;
      g_lastClockMs = nowMs;
      renderCurrent(true);
    }
    delay(20);
    return;
  }
  g_wasNightSleep = false;

  // 3. 日間背景資料同步
  bool dataUpdated = false;
  bool stockUpdated = false;
  if ((nowMs - g_lastQuotaMs) >= AppConfig::QUOTA_SYNC_INTERVAL_MS) {
    syncAllData(false, false);
    g_lastQuotaMs = nowMs;
    if (g_currentPage != PageId::STOCKS) {
      dataUpdated = true;
    }
  }
  if (isMarketOpen(g_net.nowEpoch()) && (nowMs - g_lastStockMs) >= 300000UL) {
    g_stockClient.sync(g_cfg.quotaUrl, g_cfg.quotaToken, g_net.nowEpoch(), g_stocks);
    g_lastStockMs = nowMs;
    stockUpdated = true;
    dataUpdated = true;
  }
  if ((nowMs - g_lastWeatherMs) >= AppConfig::WEATHER_SYNC_INTERVAL_MS) {
    const char* cityQ = AppConfig::CITIES[g_cfg.cityIndex % AppConfig::CITY_COUNT].query;
    g_weatherClient.sync(g_cfg.owmApiKey, cityQ, g_net.nowEpoch(), g_weather);
    g_lastWeatherMs = nowMs;
    if (g_currentPage != PageId::STOCKS || isMarketOpen(g_net.nowEpoch())) {
      dataUpdated = true;
    }
  }

  // 4. 日間 30 秒自動輪播（無 Manual Hold 時）
  const bool manualHold = (static_cast<int32_t>(g_manualHoldUntil - nowMs) > 0);
  if (g_cfg.autoCycleEnabled && !manualHold && (nowMs - g_lastCycleMs) >= AppConfig::AUTO_CYCLE_SEC * 1000UL) {
    g_currentPage = static_cast<PageId>((static_cast<uint8_t>(g_currentPage) + 1) % static_cast<uint8_t>(PageId::PAGE_COUNT));
    g_lastCycleMs = nowMs;
    g_lastClockMs = nowMs;
    renderCurrent(dataUpdated || stockUpdated);
    return;
  }

  // 5. 每 1 分鐘更新時鐘（若在 Page 5 休市期間則不主動刷屏，節省壽命與視覺干擾）
  const bool allowMinuteClock =
      !(g_currentPage == PageId::STOCKS && !isMarketOpen(g_net.nowEpoch()));
  if (dataUpdated || (allowMinuteClock && (nowMs - g_lastClockMs) >= AppConfig::CLOCK_UPDATE_INTERVAL_MS)) {
    g_lastClockMs = nowMs;
    renderCurrent(dataUpdated || stockUpdated);
  }

  delay(20);
}
