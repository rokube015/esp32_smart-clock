#pragma once
#include <cstdint>

namespace smart_clock_control {
// Caller holds the session mutex, including when admitting HTTP requests.
class DownloadWindow {
  uint32_t idle_ticks;
  uint32_t last_activity {0};
  unsigned requests {0};
  bool active {false};
public:
  explicit DownloadWindow(uint32_t ticks) : idle_ticks(ticks) {}
  bool enabled() const { return active; }
  void open(uint32_t now){ active = true; last_activity = now; }
  bool begin_request(uint32_t now){
    if(!active){ return false; }
    ++requests;
    last_activity = now;
    return true;
  }
  void end_request(uint32_t now){
    if(requests > 0){ --requests; }
    last_activity = now;
  }
  bool expire(uint32_t now){
    if(active && requests == 0 && uint32_t(now - last_activity) >= idle_ticks){
      active = false;
      return true;
    }
    return false;
  }
};
}
