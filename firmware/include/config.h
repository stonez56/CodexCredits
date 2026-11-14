#pragma once

#include <Arduino.h>

// ============================================================================
// CrowPanel ESP32-S3 5.79" E-Paper 硬體腳位定義
// 依據 Elecrow 官方 CrowPanel-ESP32-5.79-E-paper-HMI 規格
// ============================================================================
namespace HardwarePins {
  constexpr uint8_t EPD_SCK  = 12;
  constexpr uint8_t EPD_MOSI = 11;
  constexpr uint8_t EPD_CS   = 45;
  constexpr uint8_t EPD_DC   = 46;
  constexpr uint8_t EPD_RST  = 47;
  constexpr uint8_t EPD_BUSY = 48;
  constexpr uint8_t EPD_PWR  = 7;   // 面板供電控制：操作前須拉 HIGH

  // 實體按鍵與撥輪（Active LOW，使用 INPUT_PULLUP）
  constexpr uint8_t BTN_EXIT      = 1;
  constexpr uint8_t BTN_MENU      = 2;
  constexpr uint8_t ROTARY_DOWN   = 4;
  constexpr uint8_t ROTARY_CLICK  = 5;
  constexpr uint8_t ROTARY_UP     = 6;
}

// ============================================================================
// 顯示與排程常數
// ============================================================================
namespace AppConfig {
  constexpr uint16_t DISPLAY_WIDTH  = 792;
  constexpr uint16_t DISPLAY_HEIGHT = 272;
  constexpr uint16_t TOP_BAR_HEIGHT = 44;

  // 時間與時區 (UTC+8 台灣標準時間)
  constexpr char NTP_SERVER_1[] = "pool.ntp.org";
  constexpr char NTP_SERVER_2[] = "time.google.com";
  constexpr char TZ_INFO[]      = "CST-8";
  constexpr long UTC_OFFSET_SEC = 8 * 3600;

  // 自動輪播與刷新策略（毫秒 / 秒 / 次數）
  constexpr uint32_t AUTO_CYCLE_SEC          = 30;   // 日間每 30 秒切換下一頁
  constexpr uint32_t MANUAL_HOLD_SEC         = 180;  // 手動操作後暫停輪播 3 分鐘
  constexpr uint32_t NIGHT_WAKE_HOLD_SEC     = 60;   // 夜間手動喚醒保留 60 秒
  constexpr uint32_t CLOCK_UPDATE_INTERVAL_MS = 60UL * 1000UL;       // 1 分鐘
  constexpr uint32_t QUOTA_SYNC_INTERVAL_MS   = 15UL * 60UL * 1000UL; // 15 分鐘
  constexpr uint32_t WEATHER_SYNC_INTERVAL_MS = 30UL * 60UL * 1000UL; // 30 分鐘
  constexpr uint32_t NIGHT_SYNC_INTERVAL_MS   = 30UL * 60UL * 1000UL; // 夜間 30 分鐘
  constexpr uint8_t  FULL_REFRESH_EVERY_N_PARTIAL = 10; // 每 10 次局刷強制全刷除殘影

  // 夜間休眠時段 (00:00 ~ 07:00)
  constexpr uint8_t NIGHT_START_HOUR = 0;
  constexpr uint8_t NIGHT_END_HOUR   = 7;

  // 預設可選城市清單（支援選單切換）
  struct CityPreset {
    const char* query;
    const char* nameEn;
    const char* nameZh;
  };

  constexpr CityPreset CITIES[] = {
    { "Hsinchu,TW",   "Hsinchu",   "新竹" },
    { "Taipei,TW",    "Taipei",    "台北" },
    { "Taichung,TW",  "Taichung",  "台中" },
    { "Kaohsiung,TW", "Kaohsiung", "高雄" }
  };
  constexpr size_t CITY_COUNT = sizeof(CITIES) / sizeof(CITIES[0]);
  constexpr size_t DEFAULT_CITY_INDEX = 0; // Hsinchu,TW
}

