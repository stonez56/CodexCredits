#pragma once

#include <Arduino.h>

enum class InputEvent : uint8_t {
  NONE = 0,
  UP_SHORT,
  DOWN_SHORT,
  CLICK_SHORT,
  CLICK_LONG,
  MENU_SHORT,
  MENU_LONG,
  EXIT_SHORT
};

class InputHandler {
public:
  void begin();
  InputEvent poll();
  static const char* eventName(InputEvent ev);

private:
  struct ButtonState {
    uint8_t pin;
    bool lastRaw;
    bool stableState;
    uint32_t lastDebounceMs;
    uint32_t pressedAtMs;
    bool longFired;
    InputEvent shortEvent;
    InputEvent longEvent;
  };

  ButtonState buttons_[5];
  static constexpr uint32_t DEBOUNCE_MS   = 35;
  static constexpr uint32_t LONG_PRESS_MS = 1000;
};

