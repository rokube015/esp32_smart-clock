#pragma once
#include <cstdint>

namespace button_control {
// One event per stable press, including across tick wrap.
class DebouncedButton {
  uint32_t debounce_ticks;
  uint32_t changed_at {0};
  bool sampled {false};
  bool stable {false};
public:
  explicit DebouncedButton(uint32_t ticks) : debounce_ticks(ticks) {}
  bool update(bool pressed, uint32_t now){
    if(pressed != sampled){ sampled = pressed; changed_at = now; }
    if(sampled != stable && uint32_t(now - changed_at) >= debounce_ticks){
      stable = sampled;
      return stable;
    }
    return false;
  }
};
}
