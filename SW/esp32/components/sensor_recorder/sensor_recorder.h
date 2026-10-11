#pragma once
#include "sensor_data.h"
#include "sd_card.h"

class SENSOR_RECORDER {
public:
  enum class Result { SUCCESS, SKIPPED, FAILED };
  static constexpr const char PATH[] = "/sensor_log.csv";
  esp_err_t prepare(SD_CARD& sd);
  Result record(const SensorData& data, bool sd_available, SD_CARD& sd);
private:
  bool seen {false};
  uint32_t last_sequence {0};
};
