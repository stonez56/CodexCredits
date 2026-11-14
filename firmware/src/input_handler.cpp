#include "input_handler.h"
#include "config.h"

void InputHandler::begin() {
  buttons_[0] = { HardwarePins::ROTARY_UP,    HIGH, HIGH, 0, 0, false, InputEvent::UP_SHORT,    InputEvent::NONE };
  buttons_[1] = { HardwarePins::ROTARY_DOWN,  HIGH, HIGH, 0, 0, false, InputEvent::DOWN_SHORT,  InputEvent::NONE };
  buttons_[2] = { HardwarePins::ROTARY_CLICK, HIGH, HIGH, 0, 0, false, InputEvent::CLICK_SHORT, InputEvent::CLICK_LONG };
  buttons_[3] = { HardwarePins::BTN_MENU,     HIGH, HIGH, 0, 0, false, InputEvent::MENU_SHORT,  InputEvent::MENU_LONG };
  buttons_[4] = { HardwarePins::BTN_EXIT,     HIGH, HIGH, 0, 0, false, InputEvent::EXIT_SHORT,  InputEvent::NONE };

  for (auto& b : buttons_) {
    pinMode(b.pin, INPUT_PULLUP);
  }
}

InputEvent InputHandler::poll() {
  const uint32_t now = millis();

  for (auto& b : buttons_) {
    bool raw = digitalRead(b.pin);
    if (raw != b.lastRaw) {
      b.lastDebounceMs = now;
      b.lastRaw = raw;
    }

    if ((now - b.lastDebounceMs) >= DEBOUNCE_MS && raw != b.stableState) {
      b.stableState = raw;
      if (b.stableState == LOW) {
        b.pressedAtMs = now;
        b.longFired = false;
      } else {
        if (!b.longFired && b.shortEvent != InputEvent::NONE) {
          return b.shortEvent;
        }
      }
    }

    if (b.stableState == LOW && !b.longFired && b.longEvent != InputEvent::NONE) {
      if ((now - b.pressedAtMs) >= LONG_PRESS_MS) {
        b.longFired = true;
        return b.longEvent;
      }
    }
  }

  return InputEvent::NONE;
}

const char* InputHandler::eventName(InputEvent ev) {
  switch (ev) {
    case InputEvent::UP_SHORT:    return "ROTARY_UP";
    case InputEvent::DOWN_SHORT:  return "ROTARY_DOWN";
    case InputEvent::CLICK_SHORT: return "ROTARY_CLICK_SHORT";
    case InputEvent::CLICK_LONG:  return "ROTARY_CLICK_LONG";
    case InputEvent::MENU_SHORT:  return "MENU_SHORT";
    case InputEvent::MENU_LONG:   return "MENU_LONG";
    case InputEvent::EXIT_SHORT:  return "EXIT_SHORT";
    default:                      return "NONE";
  }
}

