#pragma once

#include <Arduino.h>
#include <time.h>

enum class PageId : uint8_t {
  OVERVIEW = 0,
  CODEX_DETAIL = 1,
  GEMINI_DETAIL = 2,
  WEATHER_5DAY = 3,
  PAGE_COUNT = 4
};

enum class LangMode : uint8_t {
  EN = 0,
  ZH_TW = 1
};

struct RuntimeConfig {
  char owmApiKey[48];
  char quotaUrl[160];
  char quotaToken[96];
  uint8_t cityIndex;
  LangMode lang;
  bool autoCycleEnabled;
};

struct QuotaWindow {
  uint8_t remainingPct;
  time_t expiresAtEpoch;
};

struct CodexPlusData {
  bool valid;
  QuotaWindow fiveHour;
  QuotaWindow weekly;
  bool hasCredits;
  char creditsText[25];
  uint8_t resetsAvailable;
  time_t resetEpochs[10];
  uint8_t resetEpochsCount;
};

struct GeminiAccountData {
  bool valid;
  char label[25];
  QuotaWindow weekly;
  QuotaWindow fiveHour;
};

struct QuotaState {
  bool isMock;
  bool lastSyncOk;
  time_t lastSyncEpoch;
  CodexPlusData codex;
  GeminiAccountData gemini[2];
  uint8_t geminiCount;
};

struct CurrentWeather {
  bool valid;
  char city[25];
  float tempC;
  float feelsLikeC;
  uint8_t humidity;
  float windMs;
  char icon[4];
  char desc[32];
  time_t sunriseEpoch;
  time_t sunsetEpoch;
};

struct DailyForecast {
  bool valid;
  char dateLabel[8]; // MM/DD
  uint8_t weekday;   // 0=Sun..6=Sat
  float tempMin;
  float tempMax;
  uint8_t popPct;
  char icon[4];
};

struct WeatherState {
  bool lastSyncOk;
  time_t lastSyncEpoch;
  CurrentWeather current;
  DailyForecast days[5];
  uint8_t dayCount;
};

