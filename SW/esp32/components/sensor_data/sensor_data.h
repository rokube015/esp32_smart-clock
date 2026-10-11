#pragma once
#include <cstdint>
#include <ctime>
#include <cmath>

struct SensorData {
  uint16_t co2 {0};
  float temperature {0};
  double humidity {0};
  float pressure {0};
  bool co2_valid {false};
  bool bme280_valid {false};
  std::time_t measured_at {0};
  bool time_valid {false};
  uint32_t sequence {0};
  bool bme_valid() const {
    return bme280_valid && std::isfinite(temperature) &&
        std::isfinite(humidity) && std::isfinite(pressure);
  }
  bool all_valid() const { return co2_valid && bme_valid(); }
  void complete(bool co2_ok, uint16_t ppm, bool bme_ok, float temp,
                double rh, float hpa, std::time_t when, bool clock_valid){
    co2_valid = co2_ok;
    bme280_valid = bme_ok && std::isfinite(temp) && std::isfinite(rh) && std::isfinite(hpa);
    if(co2_valid){ co2 = ppm; }
    if(bme280_valid){ temperature = temp; humidity = rh; pressure = hpa; }
    measured_at = when;
    time_valid = clock_valid;
    ++sequence;
  }
};
