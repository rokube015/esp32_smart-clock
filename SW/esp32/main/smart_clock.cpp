#include <iostream>

#include "smart_clock.h"
#include "wifi_pass.h"

void SMART_CLOCK::init(void){
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
  if(r == ESP_OK){
    r = scd40.create_task("measure_co2", 2048, 10);
  }
  // initialize sd card 
  if(r == ESP_OK){ 
    r = sd_card.init();
  }

  if(r == ESP_OK){
    char sd_card_write_data_buffer[400];
    snprintf(sd_card_write_data_buffer, sizeof(sd_card_write_data_buffer), "YYYY/MM/DD, week, HH:MM:SS, CO2[rpm], Temperature[degree], Humidity[%%], Pressure[hPa]\n");
    r = sd_card.write_data(file_path, sizeof(file_path), sd_card_write_data_buffer, 'w');
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to write data to sd_card.");
    }
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

void SMART_CLOCK::run(void){
  esp_err_t r = ESP_OK;
  char time_info[100] = "time";
  char sd_card_write_data_buffer[400];
 
  if(r == ESP_OK){
    bme280.get_all_results(&temperature, &humidity, &pressure);
  } 
  if(r == ESP_OK){
    r = sntp.get_logtime(time_info, sizeof(time_info));
  }
  if(r == ESP_OK){
    snprintf(sd_card_write_data_buffer, sizeof(sd_card_write_data_buffer), "%s, %d, %.2lf, %.2lf, %.2lf\n", time_info, co2, temperature, humidity, pressure);
    r = sd_card.write_data(file_path, sizeof(file_path), sd_card_write_data_buffer, 'a');
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to write sensor log to sd_card.");
    }
  }
  if(r == ESP_OK){
    r = scd40.get_co2(&co2);
  }
  if(r == ESP_OK){
    std::cout << "==================================================" << std::endl;
    std::cout << "Time              : " << time_info << std::endl;
    std::cout << "BME280 Temperature: " << temperature << "\u2103" << std::endl;
    std::cout << "BME280 Humidity   : " << humidity << "%" << std::endl;
    std::cout << "BME280 Pressure   : " << pressure << "hPa" << std::endl;
    std::cout << "SCD40  CO2        : " << co2 << "ppm" << std::endl;
    std::cout << "==================================================" << std::endl;;
  }
  vTaskDelay(pdMS_TO_TICKS(10000));
}

