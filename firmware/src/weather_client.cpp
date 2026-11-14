#include "weather_client.h"
#include "net_manager.h"
#include "config.h"
#include <ArduinoJson.h>

namespace {
  constexpr size_t MAX_CURRENT_JSON_BYTES  = 4096;
  constexpr size_t MAX_FORECAST_JSON_BYTES = 32768;
}

bool WeatherClient::parseCurrent(const String& jsonStr, WeatherState& state) {
  JsonDocument filter;
  filter["name"] = true;
  filter["main"]["temp"] = true;
  filter["main"]["feels_like"] = true;
  filter["main"]["humidity"] = true;
  filter["wind"]["speed"] = true;
  filter["weather"][0]["icon"] = true;
  filter["weather"][0]["description"] = true;
  filter["sys"]["sunrise"] = true;
  filter["sys"]["sunset"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonStr, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("[OWM] Current parse error: %s\n", err.c_str());
    return false;
  }

  CurrentWeather& c = state.current;
  strlcpy(c.city, doc["name"] | "Unknown", sizeof(c.city));
  c.tempC        = doc["main"]["temp"] | 0.0f;
  c.feelsLikeC   = doc["main"]["feels_like"] | 0.0f;
  c.humidity     = static_cast<uint8_t>(constrain(doc["main"]["humidity"] | 0, 0, 100));
  c.windMs       = doc["wind"]["speed"] | 0.0f;
  strlcpy(c.icon, doc["weather"][0]["icon"] | "01d", sizeof(c.icon));
  strlcpy(c.desc, doc["weather"][0]["description"] | "", sizeof(c.desc));
  c.sunriseEpoch = doc["sys"]["sunrise"] | 0;
  c.sunsetEpoch  = doc["sys"]["sunset"] | 0;
  c.valid        = true;
  return true;
}

bool WeatherClient::parseForecast(const String& jsonStr, time_t nowEpoch, WeatherState& state) {
  JsonDocument filter;
  filter["list"][0]["dt"] = true;
  filter["list"][0]["main"]["temp_min"] = true;
  filter["list"][0]["main"]["temp_max"] = true;
  filter["list"][0]["weather"][0]["icon"] = true;
  filter["list"][0]["pop"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonStr, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("[OWM] Forecast parse error: %s\n", err.c_str());
    return false;
  }

  JsonArray list = doc["list"];
  if (list.isNull()) return false;

  const long todayKey = (static_cast<long>(nowEpoch) + AppConfig::UTC_OFFSET_SEC) / 86400L;

  struct DayAccum {
    long dayKey;
    float tMin;
    float tMax;
    uint8_t popMax;
    char icon[4];
    bool hasMiddayIcon;
    char dateLabel[8];
    uint8_t weekday;
  };

  DayAccum acc[5];
  uint8_t count = 0;

  for (JsonObject item : list) {
    time_t dt = item["dt"] | 0;
    if (dt == 0) continue;

    time_t localEpoch = dt + AppConfig::UTC_OFFSET_SEC;
    long dKey = static_cast<long>(localEpoch) / 86400L;
    if (dKey <= todayKey) {
      continue; // 彙整未來 5 個完整日曆天
    }

    int idx = -1;
    for (uint8_t i = 0; i < count; ++i) {
      if (acc[i].dayKey == dKey) {
        idx = i;
        break;
      }
    }

    if (idx < 0) {
      if (count >= 5) continue;
      idx = count++;
      struct tm t;
      gmtime_r(&localEpoch, &t);
      acc[idx].dayKey = dKey;
      acc[idx].tMin = 999.0f;
      acc[idx].tMax = -999.0f;
      acc[idx].popMax = 0;
      acc[idx].hasMiddayIcon = false;
      strlcpy(acc[idx].icon, "01d", sizeof(acc[idx].icon));
      snprintf(acc[idx].dateLabel, sizeof(acc[idx].dateLabel), "%02d/%02d", t.tm_mon + 1, t.tm_mday);
      acc[idx].weekday = static_cast<uint8_t>(t.tm_wday);
    }

    float tMin = item["main"]["temp_min"] | 0.0f;
    float tMax = item["main"]["temp_max"] | 0.0f;
    float pop  = item["pop"] | 0.0f;
    const char* ico = item["weather"][0]["icon"] | "01d";

    if (tMin < acc[idx].tMin) acc[idx].tMin = tMin;
    if (tMax > acc[idx].tMax) acc[idx].tMax = tMax;
    uint8_t popPct = static_cast<uint8_t>(constrain(static_cast<int>(pop * 100.0f + 0.5f), 0, 100));
    if (popPct > acc[idx].popMax) acc[idx].popMax = popPct;

    struct tm t;
    gmtime_r(&localEpoch, &t);
    if (!acc[idx].hasMiddayIcon && t.tm_hour >= 11 && t.tm_hour <= 15) {
      strlcpy(acc[idx].icon, ico, sizeof(acc[idx].icon));
      acc[idx].hasMiddayIcon = true;
    } else if (!acc[idx].hasMiddayIcon) {
      strlcpy(acc[idx].icon, ico, sizeof(acc[idx].icon));
    }
  }

  state.dayCount = count;
  for (uint8_t i = 0; i < count; ++i) {
    DailyForecast& d = state.days[i];
    d.valid = true;
    strlcpy(d.dateLabel, acc[i].dateLabel, sizeof(d.dateLabel));
    d.weekday = acc[i].weekday;
    d.tempMin = acc[i].tMin;
    d.tempMax = acc[i].tMax;
    d.popPct  = acc[i].popMax;
    strlcpy(d.icon, acc[i].icon, sizeof(d.icon));
  }
  return count > 0;
}

bool WeatherClient::sync(const char* apiKey, const char* cityQuery, time_t nowEpoch, WeatherState& state) {
  if (NetManager::isPlaceholder(apiKey) || !cityQuery || cityQuery[0] == '\0') {
    Serial.println("[OWM] API key missing, skipping weather sync");
    state.lastSyncOk = false;
    return false;
  }

  char url[200];
  snprintf(url, sizeof(url),
           "https://api.openweathermap.org/data/2.5/weather?q=%s&units=metric&appid=%s",
           cityQuery, apiKey);

  String payload;
  if (!NetManager::httpGet(url, nullptr, payload, MAX_CURRENT_JSON_BYTES) || !parseCurrent(payload, state)) {
    state.lastSyncOk = false;
    return false;
  }

  snprintf(url, sizeof(url),
           "https://api.openweathermap.org/data/2.5/forecast?q=%s&units=metric&appid=%s",
           cityQuery, apiKey);

  payload = "";
  if (!NetManager::httpGet(url, nullptr, payload, MAX_FORECAST_JSON_BYTES) || !parseForecast(payload, nowEpoch, state)) {
    state.lastSyncOk = false;
    return false;
  }

  state.lastSyncOk = true;
  state.lastSyncEpoch = nowEpoch;
  return true;
}

