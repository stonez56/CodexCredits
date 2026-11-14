#pragma once

#include <Arduino.h>
#include "app_types.h"

class WeatherClient {
public:
  bool sync(const char* apiKey, const char* cityQuery, time_t nowEpoch, WeatherState& state);

  static bool parseCurrent(const String& jsonStr, WeatherState& state);
  static bool parseForecast(const String& jsonStr, time_t nowEpoch, WeatherState& state);
};

