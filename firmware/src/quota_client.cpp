#include "quota_client.h"
#include "net_manager.h"
#include <ArduinoJson.h>

namespace {
  constexpr size_t MAX_QUOTA_JSON_BYTES = 4096;

  uint8_t clampPct(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return static_cast<uint8_t>(v);
  }
}

void QuotaClient::loadMock(time_t nowEpoch, QuotaState& state) {
  if (nowEpoch < 1700000000) {
    nowEpoch = 1794632640; // NTP 尚未同步時的合理備援基準
  }

  state.isMock = true;
  state.lastSyncOk = true;
  state.lastSyncEpoch = nowEpoch;

  state.codex.valid = true;
  state.codex.fiveHour = { 100, nowEpoch + (3 * 3600 + 42 * 60) };
  state.codex.weekly   = { 78,  nowEpoch + (4 * 86400 + 18 * 3600 + 10 * 60) };
  state.codex.hasCredits = true;
  strlcpy(state.codex.creditsText, "$12.50", sizeof(state.codex.creditsText));
  state.codex.resetsAvailable = 2;
  state.codex.resetEpochsCount = 2;
  state.codex.resetEpochs[0] = nowEpoch + (1 * 86400 + 6 * 3600);
  state.codex.resetEpochs[1] = nowEpoch + (4 * 86400 + 12 * 3600);

  state.geminiCount = 2;
  state.gemini[0].valid = true;
  strlcpy(state.gemini[0].label, "Gemini Pro #1", sizeof(state.gemini[0].label));
  state.gemini[0].weekly   = { 100, nowEpoch + (6 * 86400 + 22 * 3600) };
  state.gemini[0].fiveHour = { 99,  nowEpoch + (3 * 3600 + 43 * 60) };

  state.gemini[1].valid = true;
  strlcpy(state.gemini[1].label, "Gemini Pro #2", sizeof(state.gemini[1].label));
  state.gemini[1].weekly   = { 85, nowEpoch + (4 * 86400 + 11 * 3600) };
  state.gemini[1].fiveHour = { 60, nowEpoch + (2 * 3600 + 15 * 60) };
}

bool QuotaClient::parsePayload(const String& jsonStr, QuotaState& state) {
  if (jsonStr.length() == 0 || jsonStr.length() > MAX_QUOTA_JSON_BYTES) {
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    Serial.printf("[QUOTA] JSON parse error: %s\n", err.c_str());
    return false;
  }

  QuotaState next = state;
  next.isMock = false;

  JsonObject codex = doc["codex_plus"];
  if (!codex.isNull()) {
    next.codex.valid = true;
    next.codex.fiveHour.remainingPct   = clampPct(codex["five_hour"]["remaining_pct"] | 0);
    next.codex.fiveHour.expiresAtEpoch = codex["five_hour"]["expires_at_epoch"] | 0;
    next.codex.weekly.remainingPct     = clampPct(codex["weekly"]["remaining_pct"] | 0);
    next.codex.weekly.expiresAtEpoch   = codex["weekly"]["expires_at_epoch"] | 0;

    next.codex.hasCredits = codex["purchased_credits"]["has_credits"] | false;
    const char* cText = codex["purchased_credits"]["display_text"] | "0";
    strlcpy(next.codex.creditsText, cText, sizeof(next.codex.creditsText));

    int avail = codex["resets"]["available_count"] | 0;
    next.codex.resetsAvailable = static_cast<uint8_t>(constrain(avail, 0, 99));

    JsonArray arr = codex["resets"]["schedule_epochs"];
    next.codex.resetEpochsCount = 0;
    for (JsonVariant v : arr) {
      if (next.codex.resetEpochsCount >= 10) break;
      next.codex.resetEpochs[next.codex.resetEpochsCount++] = v.as<time_t>();
    }
  }

  JsonArray gArr = doc["gemini_pro_accounts"];
  next.geminiCount = 0;
  for (JsonObject g : gArr) {
    if (next.geminiCount >= 2) break;
    GeminiAccountData& dst = next.gemini[next.geminiCount++];
    dst.valid = true;
    strlcpy(dst.label, g["label"] | "Gemini Pro", sizeof(dst.label));
    dst.weekly.remainingPct     = clampPct(g["weekly"]["remaining_pct"] | 0);
    dst.weekly.expiresAtEpoch   = g["weekly"]["expires_at_epoch"] | 0;
    dst.fiveHour.remainingPct   = clampPct(g["five_hour"]["remaining_pct"] | 0);
    dst.fiveHour.expiresAtEpoch = g["five_hour"]["expires_at_epoch"] | 0;
  }

  state = next;
  return true;
}

bool QuotaClient::sync(const char* url, const char* bearerToken, time_t nowEpoch, QuotaState& state) {
  if (NetManager::isPlaceholder(url)) {
    loadMock(nowEpoch, state);
    Serial.println("[QUOTA] Using built-in Mock JSON (QUOTA_API_URL not set)");
    return true;
  }

  String payload;
  if (!NetManager::httpGet(url, bearerToken, payload, MAX_QUOTA_JSON_BYTES)) {
    state.lastSyncOk = false;
    return false;
  }

  if (!parsePayload(payload, state)) {
    state.lastSyncOk = false;
    return false;
  }

  state.lastSyncOk = true;
  state.lastSyncEpoch = nowEpoch;
  return true;
}

void QuotaClient::formatCountdownHoursMin(time_t expiresAt, time_t nowEpoch, char* buf, size_t len) {
  long diff = static_cast<long>(expiresAt - nowEpoch);
  if (diff <= 0) {
    strlcpy(buf, "00:00", len);
    return;
  }
  long hrs = diff / 3600;
  long mins = (diff % 3600) / 60;
  snprintf(buf, len, "%02ld:%02ld", hrs, mins);
}

void QuotaClient::formatCountdownCompact(time_t expiresAt, time_t nowEpoch, char* buf, size_t len) {
  long diff = static_cast<long>(expiresAt - nowEpoch);
  if (diff <= 0) {
    strlcpy(buf, "Ready", len);
    return;
  }
  long days = diff / 86400;
  long hrs  = (diff % 86400) / 3600;
  long mins = (diff % 3600) / 60;
  if (days > 0) {
    snprintf(buf, len, "%ldd %ldh %02ldm", days, hrs, mins);
  } else {
    snprintf(buf, len, "%ldh %02ldm", hrs, mins);
  }
}

void QuotaClient::formatCountdownFull(time_t expiresAt, time_t nowEpoch, LangMode lang, char* buf, size_t len) {
  long diff = static_cast<long>(expiresAt - nowEpoch);
  if (diff <= 0) {
    strlcpy(buf, lang == LangMode::ZH_TW ? "已重設" : "Refreshed", len);
    return;
  }
  long days = diff / 86400;
  long hrs  = (diff % 86400) / 3600;
  long mins = (diff % 3600) / 60;

  if (lang == LangMode::ZH_TW) {
    if (days > 0) {
      snprintf(buf, len, "%ld 天 %ld 小時", days, hrs);
    } else {
      snprintf(buf, len, "%ld 小時 %ld 分", hrs, mins);
    }
  } else {
    if (days > 0) {
      snprintf(buf, len, "%ld days, %ld hours", days, hrs);
    } else {
      snprintf(buf, len, "%ld hours, %ld mins", hrs, mins);
    }
  }
}

void QuotaClient::formatEpochUtc8(time_t epoch, char* buf, size_t len) {
  struct tm t;
  localtime_r(&epoch, &t);
  snprintf(buf, len, "%04d-%02d-%02d %02d:%02d",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
}

