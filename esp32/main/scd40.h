#ifndef SCD40_H
#define SCD40_H
#include "driver/i2c_master.h"
#include "sdkconfig.h"
#include "esp_log.h"

#define PORT_NUMBER -1

typedef struct{
  uint16_t co2;
  double temperature;
  double relative_humidity;
}scd40_value_t;

typedef struct{
  uint8_t data[2];
  uint8_t crc;
}scd40_data_t;

esp_err_t init_scd40();

uint8_t calculate_scd40_crc(const uint8_t* data, uint16_t byte_size);

esp_err_t get_scd40_serial_number(uint64_t* pserial_number);

esp_err_t start_scd40_periodic_measurement();

esp_err_t get_scd40_sensor_data(scd40_value_t* scd40_value);

esp_err_t stop_scd40_periodic_measurement();

esp_err_t set_scd40_temperature_offset(double temperature_offset);
#endif 
