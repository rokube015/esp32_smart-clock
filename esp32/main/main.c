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
  
  r = init_scd40();
  ESP_ERROR_CHECK(r);
  uint64_t serial_number = 0;
  get_scd_40_serial_number(&serial_number);

  ESP_LOGI(MAIN_TAG, "SCD40 Serial Number:%llu", serial_number);
  while(1);

}
