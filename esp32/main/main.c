#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "scd40.h"

static const char* MAIN_TAG = "app_main_tag";

void app_main(void){
  esp_err_t r = ESP_OK;
  esp_log_level_set(MAIN_TAG, ESP_LOG_DEBUG);
  scd40_value_t scd40_value = {
    .co2 = 0,
    .temperature = 0.0, 
    .relative_humidity = 0.0
  };
  r = init_scd40();
  ESP_ERROR_CHECK(r);
  uint64_t serial_number = 0;
  get_scd40_serial_number(&serial_number);
  ESP_LOGI(MAIN_TAG, "SCD40 Serial Number:%llu", serial_number);
  vTaskDelay(500/ portTICK_PERIOD_MS);
  while(1){
    r = start_scd40_periodic_measurement();
    ESP_ERROR_CHECK(r);

    vTaskDelay(5000/ portTICK_PERIOD_MS);
    r = get_scd40_sensor_data(&scd40_value);
    ESP_ERROR_CHECK(r);
    ESP_LOGI(MAIN_TAG, "co2:%d, temperature:%f, humidity:%f",
        scd40_value.co2, scd40_value.temperature, scd40_value.relative_humidity);
    vTaskDelay(5000/ portTICK_PERIOD_MS);
    r = stop_scd40_periodic_measurement();
    ESP_ERROR_CHECK(r);
    vTaskDelay(1000/ portTICK_PERIOD_MS);
  }
}
