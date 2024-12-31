#include <iostream>

#include "smart_clock.h"
#include "wifi_pass.h"

void SMART_CLOCK::setup(void){
  esp_err_t r = ESP_OK;

  esp_event_loop_create_default();
  nvs_flash_init();

  wifi.set_credentials(ESP_WIFI_SSID, ESP_WIFI_PASS);
  wifi.init();

  // Initialize the I2C
  if(r == ESP_OK){ 
    r = i2c.init();
  }
  // Initialize the BME280 I2C device
  if(r == ESP_OK){ 
    r = bme280.init(&i2c);
  }
  // Initialize the SCD40 I2C device
  if(r == ESP_OK){
    r = scd40.init(&i2c);
  }
  // initialize sd card 
  if(r == ESP_OK){ 
    r = sd_card.init();
  }
}

void SMART_CLOCK::wifi_run(void){
  wifi_state = wifi.get_state();
  bool is_sntp_initialized = sntp.is_running();

  switch(wifi_state){
    case WIFI::state_e::READY_TO_CONNECT:
      ESP_LOGI(SMART_CLOCK_TAG, "wifi status: READY_TO_CONNECT");
      wifi.begin();
      break;
    case WIFI::state_e::DISCONNECTED:
      ESP_LOGI(SMART_CLOCK_TAG, "wifi status: DISCONNECTED");
      wifi.begin();
      break;
    case WIFI::state_e::CONNECTING:
      ESP_LOGI(SMART_CLOCK_TAG, "wifi status: CONNECTING");
      break;
    case WIFI::state_e::WAITING_FOR_IP:
      ESP_LOGI(SMART_CLOCK_TAG, "wifi status: WAITING_FOR_IP");
      break;
    case WIFI::state_e::ERROR:
      ESP_LOGI(SMART_CLOCK_TAG, "wifi status: ERROR");
      break;
    case WIFI::state_e::CONNECTED:
      ESP_LOGI(SMART_CLOCK_TAG, "wifi status: CONNECTED");
      if(!is_sntp_initialized){
       sntp.init();
      } 
      break;
    case WIFI::state_e::NOT_INITIALIZED:
      ESP_LOGI(SMART_CLOCK_TAG, "wifi status: NOT_INITIALIZED");
      break;
    case WIFI::state_e::INITIALIZED:
      ESP_LOGI(SMART_CLOCK_TAG, "wifi status: INITIALIZED");
      break;
  }
}
