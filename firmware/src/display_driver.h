#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>

class DisplayDriver {
public:
  void begin();
  void clearCanvas();
  Adafruit_GFX& gfx();

  void drawRingGauge(int cx, int cy, int outerR, int thickness, uint8_t pct);
  void commit(bool forceFull);
  void commitFull();
  void commitPartial();

  uint8_t partialCount() const { return partialCount_; }

private:
  uint8_t partialCount_ = 0;
};
