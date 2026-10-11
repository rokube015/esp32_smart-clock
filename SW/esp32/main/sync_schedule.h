#pragma once
#include <cstdint>

namespace smart_clock_control {
// esp_timer_get_time supplies monotonic microseconds, independent of wall-clock/tick wrap.
class SyncSchedule {
public:
  static constexpr int64_t TIMEOUT_US = 5LL * 60 * 1000000;
  static constexpr int64_t RETRY_US = 30LL * 60 * 1000000;
  bool active() const { return running; }
  bool due(int64_t now, bool date_changed) const {
    return !running && (retry_pending ? now >= retry_at : (!succeeded || date_changed));
  }
  void begin(int64_t now){ running = true; deadline = now + TIMEOUT_US; }
  bool expire(int64_t now, bool provisioning){
    if(!running || provisioning || now < deadline){ return false; }
    running = false; retry_pending = true; retry_at = now + RETRY_US;
    return true;
  }
  void success(){ running = false; succeeded = true; retry_pending = false; }
private:
  bool running {false}, succeeded {false}, retry_pending {false};
  int64_t deadline {0}, retry_at {0};
};
}
