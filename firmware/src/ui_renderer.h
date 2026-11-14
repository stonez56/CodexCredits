#pragma once

#include <Arduino.h>
#include "app_types.h"
#include "display_driver.h"

struct UiContext {
  PageId page;
  LangMode lang;
  uint8_t cityIndex;
  time_t nowEpoch;
  bool wifiConnected;
  bool isNightSleep;
  bool autoCycleActive;
  bool manualHoldActive;
  const QuotaState* quota;
  const WeatherState* weather;
  const StockState* stocks;
};

class UiRenderer {
public:
  void begin(DisplayDriver& display);
  void render(DisplayDriver& display, const UiContext& ctx, bool forceFull);
  void renderSplashScreen(DisplayDriver& display, const char* version, const char* statusText);
  void renderPortalScreen(DisplayDriver& display, const char* apName, const char* apIp);

private:
  void drawTopBar(DisplayDriver& display, const UiContext& ctx);
  void drawOverviewPage(DisplayDriver& display, const UiContext& ctx);
  void drawCodexDetailPage(DisplayDriver& display, const UiContext& ctx);
  void drawGeminiDetailPage(DisplayDriver& display, const UiContext& ctx);
  void drawWeatherPage(DisplayDriver& display, const UiContext& ctx);
  void drawStocksPage(DisplayDriver& display, const UiContext& ctx);

  void drawWeatherIcon(Adafruit_GFX& gfx, int cx, int cy, int r, const char* iconCode);
  void drawTextUtf8(int x, int y, const char* text);
};

