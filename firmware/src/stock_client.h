#pragma once

#include <Arduino.h>
#include "app_types.h"

class StockClient {
public:
  static void loadMock(time_t nowEpoch, StockState& state);
  static bool parsePayload(const String& jsonStr, StockState& state);
  static bool sync(const char* quotaUrl, const char* bearerToken, time_t nowEpoch, StockState& state);
  static void deriveStocksUrl(const char* quotaUrl, char* outUrl, size_t outSize);
};

