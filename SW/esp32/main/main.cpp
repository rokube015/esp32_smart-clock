#include <iostream>

#include "smart_clock.h"

static const char* MAIN_TAG = "main";

extern "C" void app_main(void){ 
  esp_log_level_set(MAIN_TAG, ESP_LOG_INFO); 
  esp_err_t r = ESP_OK;  
  ESP_LOGI(MAIN_TAG, "Start main"); 
  SMART_CLOCK smart_clock; 
  if(r == ESP_OK){ 
    r = smart_clock.init();
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "fail to initialize smart_clock.");
    }
  }
  if(r == ESP_OK){ 
    smart_clock.wifi_run();
  }
  if(r == ESP_OK){ 
    r = smart_clock.run();
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "fail to run smart_clock.");
    }
  }
  while(true){
    smart_clock.wifi_run();
    vTaskDelay(pdMS_TO_TICKS(30*1000));
  }
}
