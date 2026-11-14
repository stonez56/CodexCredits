#pragma once

#include <Arduino.h>
#include "app_types.h"

class QuotaClient {
public:
  bool sync(const char* url, const char* bearerToken, time_t nowEpoch, QuotaState& state);
  static bool parsePayload(const String& jsonStr, QuotaState& state);
  static void loadMock(time_t nowEpoch, QuotaState& state);

  // 倒數格式化工具
  static void formatCountdownCompact(time_t expiresAt, time_t nowEpoch, char* buf, size_t len);
  static void formatCountdownHoursMin(time_t expiresAt, time_t nowEpoch, char* buf, size_t len);
  static void formatCountdownFull(time_t expiresAt, time_t nowEpoch, LangMode lang, char* buf, size_t len);
  static void formatEpochUtc8(time_t epoch, char* buf, size_t len);
};

