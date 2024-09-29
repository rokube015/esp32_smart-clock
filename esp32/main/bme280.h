#ifndef BME280_H
#define BME280_H
#include "driver/i2c_master.h"
#include "sdkconfig.h"
#include "esp_log.h"

#define BME280_I2C_PORT_NUMBER -1

typedef struct{
  double temperature;
  double pressure;
  double humidity;
}bme280_value_t;

esp_err_t init_bme280();

esp_err_t check_bme280_chip_id();

esp_err_t get_bme280_data(bme280_value_t* bme280);
#endif
