#include "display_driver.h"
#include "config.h"
#include <EPD.h>
#include <math.h>

namespace {
  // 792 * 272 / 8 = 26928 bytes; Elecrow 官方驅動要求 27200 bytes 對齊緩衝
  uint8_t g_imageBW[27200];

  class CrowPanelGFX : public Adafruit_GFX {
  public:
    CrowPanelGFX() : Adafruit_GFX(AppConfig::DISPLAY_WIDTH, AppConfig::DISPLAY_HEIGHT) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
      if (x < 0 || x >= AppConfig::DISPLAY_WIDTH || y < 0 || y >= AppConfig::DISPLAY_HEIGHT) return;
      Paint_SetPixel(static_cast<uint16_t>(x), static_cast<uint16_t>(y), color ? BLACK : WHITE);
    }
  };

  CrowPanelGFX g_gfx;
}

void DisplayDriver::begin() {
  pinMode(HardwarePins::EPD_PWR, OUTPUT);
  digitalWrite(HardwarePins::EPD_PWR, HIGH);
  delay(20);

  EPD_GPIOInit();
  Paint_NewImage(g_imageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);

  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_Clear_R26A6H();
  partialCount_ = 0;
}

void DisplayDriver::clearCanvas() {
  Paint_Clear(WHITE);
}

Adafruit_GFX& DisplayDriver::gfx() {
  return g_gfx;
}

void DisplayDriver::drawRingGauge(int cx, int cy, int outerR, int thickness, uint8_t pct) {
  if (pct > 100) pct = 100;
  const int innerR = outerR - thickness;
  const float fillAngle = (pct / 100.0f) * 2.0f * static_cast<float>(M_PI);

  for (int dy = -outerR; dy <= outerR; ++dy) {
    for (int dx = -outerR; dx <= outerR; ++dx) {
      const int distSq = dx * dx + dy * dy;
      if (distSq > outerR * outerR || distSq < innerR * innerR) {
        continue;
      }

      float angle = atan2f(static_cast<float>(dx), static_cast<float>(-dy));
      if (angle < 0.0f) {
        angle += 2.0f * static_cast<float>(M_PI);
      }

      const int px = cx + dx;
      const int py = cy + dy;
      if (px < 0 || px >= AppConfig::DISPLAY_WIDTH || py < 0 || py >= AppConfig::DISPLAY_HEIGHT) {
        continue;
      }

      if (angle <= fillAngle && pct > 0) {
        Paint_SetPixel(px, py, BLACK);
      } else {
        if (((px + py) & 1) == 0 && (distSq <= (innerR + 2) * (innerR + 2) || distSq >= (outerR - 2) * (outerR - 2))) {
          Paint_SetPixel(px, py, BLACK);
        }
      }
    }
  }
}

void DisplayDriver::commitFull() {
  EPD_GPIOInit();
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_Clear_R26A6H();
  EPD_Display(g_imageBW);
  EPD_PartUpdate();
  EPD_SyncPrevRAM(g_imageBW);
  EPD_DeepSleep();
  partialCount_ = 0;
}

void DisplayDriver::commitPartial() {
  // DeepSleep 後必須執行 EPD_FastMode1Init() 喚醒 SSD1683；SyncPrevRAM 確保下次局刷清除舊像素
  EPD_GPIOInit();
  EPD_FastMode1Init();
  EPD_Display(g_imageBW);
  EPD_PartUpdate();
  EPD_SyncPrevRAM(g_imageBW);
  EPD_DeepSleep();
  partialCount_++;
}

void DisplayDriver::commit(bool forceFull) {
  // 雙 SSD1683 在跨區換頁時局部刷新會殘留舊像素，統一採用 FastMode1 清屏全刷確保零重疊、最高對比
  (void)forceFull;
  commitFull();
}
