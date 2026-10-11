#include <iostream>

#include "esp_pm.h"
#include "smart_clock.h"

static const char* MAIN_TAG = "main";

extern "C" void app_main(void){ 
  esp_log_level_set(MAIN_TAG, ESP_LOG_INFO); 
  esp_err_t r = ESP_OK;  
  ESP_LOGI(MAIN_TAG, "Start main"); 
#if CONFIG_PM_ENABLE && CONFIG_FREERTOS_USE_TICKLESS_IDLE
  const esp_pm_config_t pm_config = {
    .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
    .min_freq_mhz = 10,
    .light_sleep_enable = true,
  };
  r = esp_pm_configure(&pm_config);
  if(r != ESP_OK){
    ESP_LOGE(MAIN_TAG, "fail to configure automatic light sleep.");
  }
#endif
  SMART_CLOCK smart_clock; 
  if(r == ESP_OK){ 
    r = smart_clock.init();
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "fail to initialize smart_clock.");
    }
  }
  if(r == ESP_OK){ 
    r = smart_clock.run();
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "fail to run smart_clock.");
    }
  }
  while(true){
    // SMART_CLOCK owns component tasks and handles their events.
    vTaskDelay(pdMS_TO_TICKS(30 * 1000));
  }
}
