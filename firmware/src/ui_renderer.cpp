#include "ui_renderer.h"
#include "config.h"
#include "quota_client.h"
#include "tc_font.h"

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>

namespace {
  const char* weekdayEn(int wday) {
    static const char* const kDays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    return (wday >= 0 && wday < 7) ? kDays[wday] : "?";
  }

  const uint8_t* findTcGlyph(uint32_t cp) {
    for (size_t i = 0; i < TC_FONT_COUNT; ++i) {
      if (TC_FONT_GLYPHS[i].codepoint == cp) {
        return TC_FONT_GLYPHS[i].bitmap;
      }
    }
    return nullptr;
  }

  // 混排繪製粗體 ASCII (FreeSansBold9pt) + 16x16 粗體繁體中文 (tc_font.h)
  void drawMixedBold(Adafruit_GFX& gfx, int x, int yBaseline, const char* utf8) {
    gfx.setFont(&FreeSansBold9pt7b);
    gfx.setTextColor(1);
    int curX = x;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(utf8);

    while (*p) {
      if (*p == '%') {
        gfx.setCursor(curX, yBaseline);
        gfx.print("%");
        curX += 13;
        ++p;
      } else if (*p < 0x80) {
        char ch[2] = { static_cast<char>(*p), '\0' };
        int16_t bx, by;
        uint16_t bw, bh;
        gfx.getTextBounds(ch, curX, yBaseline, &bx, &by, &bw, &bh);
        gfx.setCursor(curX, yBaseline);
        gfx.print(ch);
        curX += (*p == ' ') ? 5 : (static_cast<int>(bw) + 2);
        ++p;
      } else if ((*p & 0xE0) == 0xC0 && p[1]) {
        uint32_t cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
        p += 2;
        const uint8_t* bm = findTcGlyph(cp);
        if (bm) gfx.drawBitmap(curX, yBaseline - 17, bm, TC_GLYPH_W, TC_GLYPH_H, 1);
        curX += TC_GLYPH_W + 1;
      } else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) {
        uint32_t cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        p += 3;
        const uint8_t* bm = findTcGlyph(cp);
        if (bm) gfx.drawBitmap(curX, yBaseline - 17, bm, TC_GLYPH_W, TC_GLYPH_H, 1);
        curX += TC_GLYPH_W + 1;
      } else {
        ++p;
      }
    }
  }

  // 反白卡片標題列（黑底白字）
  void drawCardHeader(DisplayDriver& disp, int x, int y, int w, int h, const char* title) {
    auto& gfx = disp.gfx();
    gfx.fillRect(x, y, w, h, 1);
    gfx.setFont(&FreeSansBold9pt7b);
    gfx.setTextColor(0);
    gfx.setCursor(x + 10, y + h - 7);
    gfx.print(title);
    gfx.setTextColor(1);
  }

  // 統一規格環形圖：[ 100% ] (O)
  void drawPctWithRing(DisplayDriver& disp, int ringCx, int ringCy, int r, int thick, uint8_t pct) {
    disp.drawRingGauge(ringCx, ringCy, r, thick, pct);
    auto& gfx = disp.gfx();
    gfx.setFont(&FreeSansBold12pt7b);
    gfx.setTextColor(1);

    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", pct);
    int16_t bx, by;
    uint16_t bw, bh;
    gfx.getTextBounds(buf, 0, 0, &bx, &by, &bw, &bh);
    gfx.setCursor(ringCx - r - 8 - static_cast<int>(bw), ringCy + static_cast<int>(bh) / 2);
    gfx.print(buf);
  }
}

void UiRenderer::begin(DisplayDriver& display) {
  (void)display;
}

void UiRenderer::drawTextUtf8(int x, int y, const char* text) {
  (void)x;
  (void)y;
  (void)text;
}

void UiRenderer::drawWeatherIcon(Adafruit_GFX& gfx, int cx, int cy, int r, const char* iconCode) {
  char code[3] = { '0', '1', '\0' };
  if (iconCode && strlen(iconCode) >= 2) {
    code[0] = iconCode[0];
    code[1] = iconCode[1];
  }

  if (strcmp(code, "01") == 0) {
    gfx.fillCircle(cx, cy, r / 2, 1);
    for (int i = 0; i < 8; ++i) {
      float a = i * (M_PI / 4.0f);
      gfx.drawLine(cx + cosf(a) * (r * 0.65f), cy + sinf(a) * (r * 0.65f),
                   cx + cosf(a) * r, cy + sinf(a) * r, 1);
    }
  } else if (strcmp(code, "02") == 0 || strcmp(code, "03") == 0 || strcmp(code, "04") == 0) {
    gfx.fillCircle(cx - r / 3, cy, r / 3, 1);
    gfx.fillCircle(cx + r / 5, cy - r / 6, r / 2.4f, 1);
    gfx.fillRect(cx - r / 2, cy, r, r / 3, 1);
  } else if (strcmp(code, "09") == 0 || strcmp(code, "10") == 0) {
    gfx.fillCircle(cx - r / 4, cy - r / 5, r / 3, 1);
    gfx.fillCircle(cx + r / 5, cy - r / 4, r / 2.5f, 1);
    gfx.fillRect(cx - r / 2, cy - r / 5, r, r / 4, 1);
    for (int i = -1; i <= 1; ++i) {
      gfx.drawLine(cx + i * (r / 3), cy + r / 5, cx + i * (r / 3) - 3, cy + r * 0.7f, 1);
    }
  } else if (strcmp(code, "11") == 0) {
    gfx.fillRect(cx - r / 2, cy - r / 2, r, r / 2, 1);
    gfx.drawLine(cx, cy, cx - 4, cy + r / 2, 1);
    gfx.drawLine(cx - 4, cy + r / 2, cx + 4, cy + r / 2, 1);
    gfx.drawLine(cx + 4, cy + r / 2, cx, cy + r, 1);
  } else {
    for (int i = -1; i <= 1; ++i) {
      gfx.fillRect(cx - r / 2, cy + i * 6, r, 2, 1);
    }
  }
}

void UiRenderer::drawTopBar(DisplayDriver& display, const UiContext& ctx) {
  auto& gfx = display.gfx();
  gfx.drawRect(2, 2, AppConfig::DISPLAY_WIDTH - 4, AppConfig::DISPLAY_HEIGHT - 4, 1);
  gfx.drawRect(3, 3, AppConfig::DISPLAY_WIDTH - 6, AppConfig::DISPLAY_HEIGHT - 6, 1);
  gfx.fillRect(2, AppConfig::TOP_BAR_HEIGHT, AppConfig::DISPLAY_WIDTH - 4, 2, 1);

  struct tm t;
  localtime_r(&ctx.nowEpoch, &t);

  gfx.setFont(&FreeSansBold9pt7b);
  gfx.setTextColor(1);

  // 左側：UTC+8 日期時間
  char timeBuf[40];
  if (ctx.nowEpoch > 1700000000) {
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %s %02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             weekdayEn(t.tm_wday), t.tm_hour, t.tm_min);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "NTP Syncing...");
  }
  gfx.setCursor(14, 29);
  gfx.print(timeBuf);

  // 中段：城市 + 即時氣溫與濕度 + 天氣圖示 (緊湊排版防止疊字)
  const auto& city = AppConfig::CITIES[ctx.cityIndex % AppConfig::CITY_COUNT];
  char wBuf[48];
  if (ctx.weather && ctx.weather->current.valid) {
    snprintf(wBuf, sizeof(wBuf), "%s %.1fC %u%%",
             ctx.lang == LangMode::ZH_TW ? city.nameZh : city.nameEn,
             ctx.weather->current.tempC,
             ctx.weather->current.humidity);
    drawMixedBold(gfx, 240, 29, wBuf);
    // 天氣圖示緊鄰文字右側 (240 + ~160px = 405px)
    drawWeatherIcon(gfx, 412, 23, 11, ctx.weather->current.icon);
  } else {
    drawMixedBold(gfx, 240, 29, ctx.lang == LangMode::ZH_TW ? city.nameZh : city.nameEn);
  }

  // 右側：狀態膠囊 + 頁碼
  const char* modeTag = ctx.isNightSleep ? "SLEEP" : (ctx.manualHoldActive ? "HOLD" : (ctx.autoCycleActive ? "AUTO" : "LOCK"));
  const char* srcTag  = (ctx.quota && ctx.quota->isMock) ? "MOCK" : ((ctx.quota && ctx.quota->lastSyncOk) ? "LIVE" : "ERR");
  const char* langTag = (ctx.lang == LangMode::ZH_TW) ? "ZH" : "EN";

  char statusBuf[64];
  snprintf(statusBuf, sizeof(statusBuf), "%s | %s | %s | P%u/%u | %s",
           modeTag, srcTag, langTag,
           static_cast<unsigned>(ctx.page) + 1,
           static_cast<unsigned>(PageId::PAGE_COUNT),
           AppConfig::APP_VERSION);
  gfx.setFont(&FreeSansBold9pt7b);
  int16_t bx, by;
  uint16_t bw, bh;
  gfx.getTextBounds(statusBuf, 0, 0, &bx, &by, &bw, &bh);
  gfx.setCursor(AppConfig::DISPLAY_WIDTH - 10 - static_cast<int>(bw), 29);
  gfx.print(statusBuf);
}

void UiRenderer::drawOverviewPage(DisplayDriver& display, const UiContext& ctx) {
  auto& gfx = display.gfx();
  const int bodyTop = AppConfig::TOP_BAR_HEIGHT + 2;
  const int colW = 264;

  gfx.fillRect(colW, bodyTop, 2, AppConfig::DISPLAY_HEIGHT - bodyTop - 2, 1);
  gfx.fillRect(colW * 2, bodyTop, 2, AppConfig::DISPLAY_HEIGHT - bodyTop - 2, 1);

  const QuotaState* q = ctx.quota;
  if (!q) return;
  char buf[48];

  // --- 欄位 1: CODEX PLUS ---
  drawCardHeader(display, 4, bodyTop, colW - 4, 26, "CODEX PLUS");
  if (q->codex.valid) {
    drawMixedBold(gfx, 16, 94, ctx.lang == LangMode::ZH_TW ? "5小時限制" : "5hr Limit");
    QuotaClient::formatCountdownHoursMin(q->codex.fiveHour.expiresAtEpoch, ctx.nowEpoch, buf, sizeof(buf));
    gfx.setFont(&FreeSansBold12pt7b);
    gfx.setCursor(16, 118);
    gfx.print(buf);
    drawPctWithRing(display, 232, 102, 17, 4, q->codex.fiveHour.remainingPct);

    gfx.drawLine(14, 130, colW - 14, 130, 1);

    drawMixedBold(gfx, 16, 152, ctx.lang == LangMode::ZH_TW ? "每週限制" : "Weekly Limit");
    QuotaClient::formatCountdownCompact(q->codex.weekly.expiresAtEpoch, ctx.nowEpoch, buf, sizeof(buf));
    gfx.setFont(&FreeSansBold9pt7b);
    gfx.setCursor(16, 174);
    gfx.print(buf);
    drawPctWithRing(display, 232, 158, 17, 4, q->codex.weekly.remainingPct);

    gfx.drawLine(14, 188, colW - 14, 188, 1);

    snprintf(buf, sizeof(buf), "%s: %s",
             ctx.lang == LangMode::ZH_TW ? "購買點數" : "Credits",
             q->codex.hasCredits ? q->codex.creditsText : "0");
    drawMixedBold(gfx, 16, 214, buf);

    snprintf(buf, sizeof(buf), "%s: %u",
             ctx.lang == LangMode::ZH_TW ? "可用重設" : "Resets",
             q->codex.resetsAvailable);
    drawMixedBold(gfx, 16, 242, buf);
  }

  // --- 欄位 2 & 3: GEMINI PRO #1 & #2 ---
  for (uint8_t i = 0; i < 2; ++i) {
    const int xBase = colW * (i + 1);
    const int x0 = xBase + 16;
    const int ringX = xBase + 232;
    const GeminiAccountData& g = q->gemini[i];

    drawCardHeader(display, xBase + 2, bodyTop, colW - 4, 26,
                   g.valid ? g.label : (i == 0 ? "GEMINI PRO #1" : "GEMINI PRO #2"));
    if (!g.valid) continue;

    drawMixedBold(gfx, x0, 98, ctx.lang == LangMode::ZH_TW ? "每週剩餘" : "Weekly Limit");
    QuotaClient::formatCountdownCompact(g.weekly.expiresAtEpoch, ctx.nowEpoch, buf, sizeof(buf));
    gfx.setFont(&FreeSansBold12pt7b);
    gfx.setCursor(x0, 124);
    gfx.print(buf);
    drawPctWithRing(display, ringX, 110, 17, 4, g.weekly.remainingPct);

    gfx.drawLine(xBase + 14, 152, xBase + colW - 14, 152, 1);

    drawMixedBold(gfx, x0, 180, ctx.lang == LangMode::ZH_TW ? "5小時剩餘" : "5hr Limit");
    QuotaClient::formatCountdownHoursMin(g.fiveHour.expiresAtEpoch, ctx.nowEpoch, buf, sizeof(buf));
    gfx.setFont(&FreeSansBold12pt7b);
    gfx.setCursor(x0, 206);
    gfx.print(buf);
    drawPctWithRing(display, ringX, 192, 17, 4, g.fiveHour.remainingPct);

    if (g.poolCount >= 3) {
      gfx.drawLine(xBase + 14, 224, xBase + colW - 14, 224, 1);
      gfx.setFont(&FreeSansBold9pt7b);
      gfx.setCursor(x0, 248);
      gfx.printf("C/GPT:%u%%  %s:%u%%",
                 g.pools[1].weekly.remainingPct,
                 strncmp(g.pools[2].name, "Gemini 4 ", 9) == 0 ? g.pools[2].name + 9 : g.pools[2].name,
                 g.pools[2].weekly.remainingPct);
    }
  }
}

void UiRenderer::drawCodexDetailPage(DisplayDriver& display, const UiContext& ctx) {
  auto& gfx = display.gfx();
  const int bodyTop = AppConfig::TOP_BAR_HEIGHT + 2;
  const int splitX = 330;
  gfx.fillRect(splitX, bodyTop, 2, AppConfig::DISPLAY_HEIGHT - bodyTop - 2, 1);

  const CodexPlusData& c = ctx.quota->codex;
  char buf[64];

  drawCardHeader(display, 4, bodyTop, splitX - 4, 26, "CODEX PLUS DETAIL");

  drawMixedBold(gfx, 16, 98, ctx.lang == LangMode::ZH_TW ? "5小時倒數" : "5hr Expire");
  QuotaClient::formatCountdownHoursMin(c.fiveHour.expiresAtEpoch, ctx.nowEpoch, buf, sizeof(buf));
  gfx.setFont(&FreeSansBold12pt7b);
  gfx.setCursor(16, 124);
  gfx.print(buf);
  drawPctWithRing(display, 290, 110, 22, 5, c.fiveHour.remainingPct);

  gfx.drawLine(14, 144, splitX - 14, 144, 1);

  drawMixedBold(gfx, 16, 170, ctx.lang == LangMode::ZH_TW ? "每週倒數" : "Weekly Expire");
  QuotaClient::formatCountdownCompact(c.weekly.expiresAtEpoch, ctx.nowEpoch, buf, sizeof(buf));
  gfx.setFont(&FreeSansBold12pt7b);
  gfx.setCursor(16, 196);
  gfx.print(buf);
  drawPctWithRing(display, 290, 182, 22, 5, c.weekly.remainingPct);

  gfx.drawLine(14, 214, splitX - 14, 214, 1);

  snprintf(buf, sizeof(buf), "%s: %s",
           ctx.lang == LangMode::ZH_TW ? "已購點數" : "Exact Credits",
           c.hasCredits ? c.creditsText : "0");
  drawMixedBold(gfx, 16, 246, buf);

  // 右側：用量上限重設明細
  snprintf(buf, sizeof(buf), "QUOTA RESETS  (%u AVAILABLE)", c.resetsAvailable);
  drawCardHeader(display, splitX + 2, bodyTop, AppConfig::DISPLAY_WIDTH - splitX - 6, 26, buf);

  if (ctx.lang == LangMode::ZH_TW) {
    snprintf(buf, sizeof(buf), "用量上限重設 (可用 %u 次)", c.resetsAvailable);
    drawMixedBold(gfx, splitX + 18, 96, buf);
  }

  const int startY = (ctx.lang == LangMode::ZH_TW) ? 124 : 102;
  gfx.setFont(&FreeSansBold9pt7b);
  for (uint8_t i = 0; i < c.resetEpochsCount && i < 6; ++i) {
    char tBuf[24];
    QuotaClient::formatEpochUtc8(c.resetEpochs[i], tBuf, sizeof(tBuf));
    gfx.setCursor(splitX + 20, startY + i * 24);
    gfx.printf("#%u   %s  (UTC+8)", i + 1, tBuf);
  }
  if (c.resetEpochsCount == 0) {
    drawMixedBold(gfx, splitX + 20, startY, ctx.lang == LangMode::ZH_TW ? "尚無排程重設紀錄" : "No scheduled resets");
  }
}

void UiRenderer::drawGeminiDetailPage(DisplayDriver& display, const UiContext& ctx) {
  auto& gfx = display.gfx();
  const int bodyTop = AppConfig::TOP_BAR_HEIGHT + 2;
  const int halfX = 396;
  gfx.fillRect(halfX, bodyTop, 2, AppConfig::DISPLAY_HEIGHT - bodyTop - 2, 1);

  for (uint8_t i = 0; i < 2; ++i) {
    const int xBase = (i == 0) ? 0 : halfX;
    const int x0 = xBase + 16;
    const GeminiAccountData& g = ctx.quota->gemini[i];

    drawCardHeader(display, xBase + 4, bodyTop, halfX - 6, 26,
                   g.valid ? g.label : "GEMINI PRO");
    if (!g.valid) continue;

    // 動態繪製該帳號下的所有模型池 (Gemini Models, Claude & GPT, Gemini 4 Argon 或未來新名稱)
    int y = 96;
    for (uint8_t p = 0; p < g.poolCount && p < 4; ++p) {
      const GeminiPoolData& pool = g.pools[p];
      if (!pool.valid) continue;

      if (p > 0) {
        gfx.drawLine(xBase + 14, y - 22, xBase + halfX - 14, y - 22, 1);
      }

      gfx.setFont(&FreeSansBold9pt7b);
      gfx.setTextColor(1);
      gfx.setCursor(x0, y);
      gfx.print(pool.name);

      // 右側：顯示 Weekly (W) 與 5hr (5h) 百分比與圓環
      if (pool.hasWeekly && pool.hasFiveHour) {
        gfx.setCursor(xBase + 184, y);
        gfx.printf("W:%u%%", pool.weekly.remainingPct);
        display.drawRingGauge(xBase + 258, y - 5, 12, 3, pool.weekly.remainingPct);

        gfx.setCursor(xBase + 286, y);
        gfx.printf("5h:%u%%", pool.fiveHour.remainingPct);
        display.drawRingGauge(xBase + 368, y - 5, 12, 3, pool.fiveHour.remainingPct);
      } else if (pool.hasWeekly) {
        gfx.setCursor(xBase + 266, y);
        gfx.printf("W:%u%%", pool.weekly.remainingPct);
        display.drawRingGauge(xBase + 368, y - 5, 12, 3, pool.weekly.remainingPct);
      }

      // 第二行顯示完整倒數（若有）
      char wBuf[32] = "";
      char hBuf[24] = "";
      if (pool.hasWeekly) {
        QuotaClient::formatCountdownFull(pool.weekly.expiresAtEpoch, ctx.nowEpoch, ctx.lang, wBuf, sizeof(wBuf));
      }
      if (pool.hasFiveHour) {
        QuotaClient::formatCountdownHoursMin(pool.fiveHour.expiresAtEpoch, ctx.nowEpoch, hBuf, sizeof(hBuf));
      }

      char subLine[64];
      if (pool.hasWeekly && pool.hasFiveHour) {
        snprintf(subLine, sizeof(subLine), "%s: %s  |  5h: %s",
                 ctx.lang == LangMode::ZH_TW ? "週重設" : "W-Reset", wBuf, hBuf);
      } else if (pool.hasWeekly) {
        snprintf(subLine, sizeof(subLine), "%s: %s",
                 ctx.lang == LangMode::ZH_TW ? "週重設" : "Weekly Reset", wBuf);
      } else {
        subLine[0] = '\0';
      }
      if (subLine[0] != '\0') {
        drawMixedBold(gfx, x0, y + 22, subLine);
      }
      y += 60;
    }
  }
}

void UiRenderer::drawWeatherPage(DisplayDriver& display, const UiContext& ctx) {
  auto& gfx = display.gfx();
  const int bodyTop = AppConfig::TOP_BAR_HEIGHT + 2;
  const int leftW = 232;
  gfx.fillRect(leftW, bodyTop, 2, AppConfig::DISPLAY_HEIGHT - bodyTop - 2, 1);

  const WeatherState* w = ctx.weather;
  const auto& city = AppConfig::CITIES[ctx.cityIndex % AppConfig::CITY_COUNT];

  drawCardHeader(display, 4, bodyTop, leftW - 4, 26, city.nameEn);
  if (w && w->current.valid) {
    drawWeatherIcon(gfx, 186, 108, 22, w->current.icon);

    gfx.setFont(&FreeSansBold18pt7b);
    gfx.setCursor(16, 120);
    gfx.printf("%.1fC", w->current.tempC);

    char buf[48];
    snprintf(buf, sizeof(buf), "%s %.1fC | %s %u%%",
             ctx.lang == LangMode::ZH_TW ? "體感" : "Feels", w->current.feelsLikeC,
             ctx.lang == LangMode::ZH_TW ? "濕度" : "Hum", w->current.humidity);
    drawMixedBold(gfx, 16, 160, buf);

    snprintf(buf, sizeof(buf), "%s %.1f m/s",
             ctx.lang == LangMode::ZH_TW ? "風速" : "Wind", w->current.windMs);
    drawMixedBold(gfx, 16, 188, buf);

    gfx.setFont(&FreeSansBold9pt7b);
    gfx.setCursor(16, 220);
    gfx.print(w->current.desc);
  } else {
    drawMixedBold(gfx, 16, 130, ctx.lang == LangMode::ZH_TW ? "等待氣象同步..." : "Waiting Weather...");
  }

  // 右側 5 日預報
  drawCardHeader(display, leftW + 2, bodyTop, AppConfig::DISPLAY_WIDTH - leftW - 6, 26, "5-DAY FORECAST (UTC+8)");
  const int colW = (AppConfig::DISPLAY_WIDTH - leftW) / 5;
  for (uint8_t i = 0; i < 5; ++i) {
    int cx0 = leftW + i * colW;
    if (i > 0) {
      gfx.drawLine(cx0, bodyTop + 26, cx0, AppConfig::DISPLAY_HEIGHT - 4, 1);
    }
    if (!w || i >= w->dayCount || !w->days[i].valid) continue;
    const DailyForecast& d = w->days[i];

    gfx.setFont(&FreeSansBold9pt7b);
    gfx.setCursor(cx0 + 10, 96);
    gfx.printf("%s %s", d.dateLabel, weekdayEn(d.weekday));

    drawWeatherIcon(gfx, cx0 + colW / 2, 142, 20, d.icon);

    gfx.setFont(&FreeSansBold9pt7b);
    gfx.setCursor(cx0 + 14, 202);
    gfx.printf("%.0f/%.0fC", d.tempMax, d.tempMin);

    gfx.setCursor(cx0 + 14, 232);
    gfx.printf("PoP %u%%", d.popPct);
  }
}

void UiRenderer::drawStocksPage(DisplayDriver& display, const UiContext& ctx) {
  auto& gfx = display.gfx();
  const int bodyTop = AppConfig::TOP_BAR_HEIGHT + 2;
  const int halfW = AppConfig::DISPLAY_WIDTH / 2;

  gfx.fillRect(halfW - 1, bodyTop, 2, AppConfig::DISPLAY_HEIGHT - bodyTop - 2, 1);

  drawCardHeader(display, 4, bodyTop, halfW - 6, 24,
                 ctx.lang == LangMode::ZH_TW ? "台股與 ETF (1/2)" : "TW ETF & TSMC (1/2)");
  drawCardHeader(display, halfW + 2, bodyTop, halfW - 6, 24,
                 ctx.lang == LangMode::ZH_TW ? "債券 ETF 與指數 (2/2)" : "BOND & INDEX (2/2)");

  const StockState* st = ctx.stocks;
  if (!st || st->itemCount == 0) {
    drawMixedBold(gfx, 24, bodyTop + 60, ctx.lang == LangMode::ZH_TW ? "等待市場報價同步..." : "Waiting stock sync...");
    return;
  }

  const int rowTop0 = bodyTop + 28;
  const int rowH = 40;
  for (uint8_t i = 0; i < st->itemCount && i < 10; ++i) {
    const auto& it = st->items[i];
    const int col = i / 5;
    const int row = i % 5;
    const int xCol = (col == 0) ? 10 : (halfW + 8);
    const int yTop = rowTop0 + row * rowH;
    const int yBase = yTop + 26;

    if (row < 4) {
      gfx.drawFastHLine(xCol, yTop + rowH - 3, halfW - 20, 1);
    }

    if (ctx.lang == LangMode::ZH_TW) {
      gfx.setFont(&FreeSansBold9pt7b);
      gfx.setTextColor(1);
      gfx.setCursor(xCol, yBase);
      gfx.print(it.code);
      drawMixedBold(gfx, xCol + 66, yBase, it.zhName);
    } else {
      gfx.setFont(&FreeSansBold9pt7b);
      gfx.setTextColor(1);
      gfx.setCursor(xCol, yBase);
      gfx.print(it.enName);
    }

    char priceBuf[16];
    char pctBuf[16];
    if (it.valid) {
      if (it.decimals == 0) {
        snprintf(priceBuf, sizeof(priceBuf), "%.0f", it.price);
      } else if (it.decimals == 1) {
        snprintf(priceBuf, sizeof(priceBuf), "%.1f", it.price);
      } else {
        snprintf(priceBuf, sizeof(priceBuf), "%.2f", it.price);
      }
      snprintf(pctBuf, sizeof(pctBuf), "%+.2f%%", it.changePct);
    } else {
      snprintf(priceBuf, sizeof(priceBuf), "--");
      snprintf(pctBuf, sizeof(pctBuf), "--");
    }

    int16_t bx, by;
    uint16_t bw, bh;
    gfx.setFont(&FreeSansBold9pt7b);
    gfx.setTextColor(1);

    gfx.getTextBounds(priceBuf, 0, 0, &bx, &by, &bw, &bh);
    gfx.setCursor(xCol + 306 - static_cast<int>(bw), yBase);
    gfx.print(priceBuf);

    gfx.getTextBounds(pctBuf, 0, 0, &bx, &by, &bw, &bh);
    gfx.setCursor(xCol + 376 - static_cast<int>(bw), yBase);
    gfx.print(pctBuf);
  }
}

void UiRenderer::render(DisplayDriver& display, const UiContext& ctx, bool forceFull) {
  display.clearCanvas();
  drawTopBar(display, ctx);

  switch (ctx.page) {
    case PageId::OVERVIEW:      drawOverviewPage(display, ctx); break;
    case PageId::CODEX_DETAIL:  drawCodexDetailPage(display, ctx); break;
    case PageId::GEMINI_DETAIL: drawGeminiDetailPage(display, ctx); break;
    case PageId::WEATHER_5DAY:  drawWeatherPage(display, ctx); break;
    case PageId::STOCKS:        drawStocksPage(display, ctx); break;
    default:                    drawOverviewPage(display, ctx); break;
  }

  display.commit(forceFull);
}

void UiRenderer::renderSplashScreen(DisplayDriver& display, const char* version, const char* statusText) {
  display.clearCanvas();
  auto& gfx = display.gfx();
  gfx.drawRect(4, 4, AppConfig::DISPLAY_WIDTH - 8, AppConfig::DISPLAY_HEIGHT - 8, 1);
  gfx.drawRect(6, 6, AppConfig::DISPLAY_WIDTH - 12, AppConfig::DISPLAY_HEIGHT - 12, 1);

  gfx.setTextColor(1);
  gfx.setFont(&FreeSansBold18pt7b);
  gfx.setCursor(48, 92);
  gfx.printf("CodexCredits  %s", version);

  gfx.setFont(&FreeSansBold12pt7b);
  gfx.setCursor(48, 140);
  gfx.print("CrowPanel 5.79\" E-Paper AI & Weather Monitor");

  gfx.fillRect(48, 166, AppConfig::DISPLAY_WIDTH - 96, 3, 1);

  gfx.setFont(&FreeSansBold12pt7b);
  gfx.setCursor(48, 212);
  gfx.print(statusText);

  display.commitFull();
}

void UiRenderer::renderPortalScreen(DisplayDriver& display, const char* apName, const char* apIp) {
  display.clearCanvas();
  auto& gfx = display.gfx();
  gfx.drawRect(4, 4, AppConfig::DISPLAY_WIDTH - 8, AppConfig::DISPLAY_HEIGHT - 8, 1);

  gfx.setFont(&FreeSansBold18pt7b);
  gfx.setCursor(28, 64);
  gfx.print("Wi-Fi Setup Mode (Captive Portal)");

  gfx.setFont(&FreeSansBold12pt7b);
  gfx.setCursor(28, 120);
  gfx.printf("1. Connect phone/PC to Wi-Fi: %s", apName);

  gfx.setCursor(28, 165);
  gfx.printf("2. Open browser: http://%s", apIp);

  gfx.setCursor(28, 210);
  gfx.print("3. Enter Wi-Fi, OpenWeather Key & Quota URL");

  display.commitFull();
}
