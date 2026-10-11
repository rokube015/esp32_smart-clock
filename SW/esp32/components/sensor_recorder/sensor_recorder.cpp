#include "sensor_recorder.h"
#include <cstdio>

esp_err_t SENSOR_RECORDER::prepare(SD_CARD& sd){
  char header[] = "YYYY/MM/DD, week, HH:MM:SS, CO2[ppm], Temperature[degree], Humidity[%], Pressure[hPa]\n";
  return sd.write_data(PATH, sizeof(PATH), header, 'h');
}

SENSOR_RECORDER::Result SENSOR_RECORDER::record(const SensorData& data, bool sd_available, SD_CARD& sd){
  // Consume even ineligible snapshots: they must never be backfilled later.
  if(seen && (data.sequence == last_sequence ||
      uint32_t(data.sequence - last_sequence) >= 0x80000000u)){ return Result::SKIPPED; }
  seen = true;
  last_sequence = data.sequence;
  if(!data.time_valid || !sd_available || !data.all_valid()){ return Result::SKIPPED; }
  std::tm local {};
  char timestamp[100] {}, row[400] {};
  if(localtime_r(&data.measured_at, &local) == nullptr ||
     std::strftime(timestamp, sizeof(timestamp), "%Y/%m/%d, %a, %H:%M:%S", &local) == 0){
    return Result::FAILED;
  }
  const int length = snprintf(row, sizeof(row), "%s, %d, %.2lf, %.2lf, %.2lf\n",
      timestamp, data.co2, data.temperature, data.humidity, data.pressure);
  if(length < 0 || static_cast<size_t>(length) >= sizeof(row)){ return Result::FAILED; }
  return sd.write_data(PATH, sizeof(PATH), row, 'a') == ESP_OK ? Result::SUCCESS : Result::FAILED;
}
