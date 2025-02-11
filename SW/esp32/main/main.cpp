#include <iostream>

#include "smart_clock.h"

static const char* MAIN_TAG = "main";

extern "C" void app_main(void){ 
  esp_log_level_set(MAIN_TAG, ESP_LOG_INFO); 
  esp_err_t r = ESP_OK;  
  ESP_LOGI(MAIN_TAG, "Start main"); 
  SMART_CLOCK smart_clock; 
  smart_clock.init();
  smart_clock.wifi_run();
  smart_clock.run();
 
  while(true){
    smart_clock.wifi_run();
    vTaskDelay(pdMS_TO_TICKS(30*1000));
  }
}
