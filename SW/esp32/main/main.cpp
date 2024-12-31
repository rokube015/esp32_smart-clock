#include <iostream>

#include "smart_clock.h"

static const char* MAIN_TAG = "main";

extern "C" void app_main(void){ 
  esp_log_level_set(MAIN_TAG, ESP_LOG_DEBUG); 
  esp_err_t r = ESP_OK;  
  ESP_LOGI(MAIN_TAG, "Start main"); 
  SMART_CLOCK smart_clock; 
  smart_clock.setup();

  float bme280_temperature{};
  float bme280_pressure{};
  double bme280_humidity{};
  uint16_t scd40_co2{0};
  double scd40_temperature{0};
  double scd40_humidity{0};
  char sd_card_write_data_buffer[400];
  const char file_path[] = "/sensor_log.csv";
  
  if(r == ESP_OK){
    snprintf(sd_card_write_data_buffer, sizeof(sd_card_write_data_buffer), "YYYY/MM/DD, week, HH:MM:SS, CO2[rpm], Temperature[\u2103], Humidity[%%RH], Pressure[hPa]\n");
    r = smart_clock.sd_card.write_data(file_path, sizeof(file_path), sd_card_write_data_buffer, 'w');
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "fail to write data to sd_card.");
    }
  }
  vTaskDelay(pdMS_TO_TICKS(50));
  char time_info[100] = "time";

  while(true){
    smart_clock.wifi_run();

    smart_clock.bme280.get_all_results(&bme280_temperature, &bme280_humidity, &bme280_pressure);
    if(r == ESP_OK){
     r = smart_clock.scd40.start_periodic_measurement();
    }
    if(r == ESP_OK){
     vTaskDelay(pdMS_TO_TICKS(5000));
     r = smart_clock.scd40.get_sensor_data(&scd40_co2, &scd40_temperature, &scd40_humidity);
    }
    if(r == ESP_OK){
     vTaskDelay(pdMS_TO_TICKS(5));
     r = smart_clock.scd40.stop_periodic_measurement();
    } 
    if(r == ESP_OK){
      r = smart_clock.sntp.get_logtime(time_info, sizeof(time_info));
    }
    if(r == ESP_OK){
      snprintf(sd_card_write_data_buffer, sizeof(sd_card_write_data_buffer), "%s, %d, %.2lf, %.2lf, %.2lf\n", time_info, scd40_co2, bme280_temperature, bme280_humidity, bme280_pressure);
      r = smart_clock.sd_card.write_data(file_path, sizeof(file_path), sd_card_write_data_buffer, 'a');
      if(r != ESP_OK){
        ESP_LOGE(MAIN_TAG, "fail to write sensor log to sd_card.");
      }
    }
    if(r == ESP_OK){
      std::cout << "==================================================" << std::endl;
      std::cout << "Time              : " << time_info << std::endl;
      std::cout << "BME280 Temperature: " << bme280_temperature << "\u2103" << std::endl;
      std::cout << "BME280 Humidity   : " << bme280_humidity << "%" << std::endl;
      std::cout << "BME280 Pressure   : " << bme280_pressure << "hPa" << std::endl;
      std::cout << "SCD40  CO2        : " << scd40_co2 << "ppm" << std::endl;
      std::cout << "==================================================" << std::endl;;
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
