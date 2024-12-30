#include <iostream>

#include "esp_log.h"
#include "esp_check.h"

#include "i2c_base.h"
#include "bme280.h"
#include "scd40.h"
#include "sd_card.h"

#include "main.h"
#include "wifi_pass.h"

static const char* MAIN_TAG = "main";

void MAIN::run(void){
  wifi_state = wifi.get_state();
  bool is_sntp_initialized = sntp.is_running();

  switch(wifi_state){
    case WIFI::state_e::READY_TO_CONNECT:
      ESP_LOGI(MAIN_TAG, "wifi status: READY_TO_CONNECT");
      wifi.begin();
      break;
    case WIFI::state_e::DISCONNECTED:
      ESP_LOGI(MAIN_TAG, "wifi status: DISCONNECTED");
      wifi.begin();
      break;
    case WIFI::state_e::CONNECTING:
      ESP_LOGI(MAIN_TAG, "wifi status: CONNECTING");
      break;
    case WIFI::state_e::WAITING_FOR_IP:
      ESP_LOGI(MAIN_TAG, "wifi status: WAITING_FOR_IP");
      break;
    case WIFI::state_e::ERROR:
      ESP_LOGI(MAIN_TAG, "wifi status: ERROR");
      break;
    case WIFI::state_e::CONNECTED:
      ESP_LOGI(MAIN_TAG, "wifi status: CONNECTED");
      if(!is_sntp_initialized){
       sntp.init();
      } 
      break;
    case WIFI::state_e::NOT_INITIALIZED:
      ESP_LOGI(MAIN_TAG, "wifi status: NOT_INITIALIZED");
      break;
    case WIFI::state_e::INITIALIZED:
      ESP_LOGI(MAIN_TAG, "wifi status: INITIALIZED");
      break;
  }
}

void MAIN::setup(void){
  esp_event_loop_create_default();
  nvs_flash_init();

  wifi.set_credentials(ESP_WIFI_SSID, ESP_WIFI_PASS);
  wifi.init();
}


MAIN app;

extern "C" void app_main(void){ 
  esp_log_level_set(MAIN_TAG, ESP_LOG_DEBUG); 
  esp_err_t r = ESP_OK;  
  ESP_LOGI(MAIN_TAG, "Start main"); 
  app.setup();
  

  //instance compornent class
  i2c_base::I2C i2c;
  BME280 Bme280;
  SCD40 Scd40;
  SD_CARD Sd_card;

  // Initialize the I2C
  if(r == ESP_OK){ 
    r = i2c.init();
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "fail to init i2c port.");
    }
  }
  
  // Initialize the BME280 I2C device
  if(r == ESP_OK){ 
    r = Bme280.init(&i2c);
    if(r != ESP_OK){ 
      ESP_LOGE(MAIN_TAG, "fail to init bme280.");
    }
  }
  if(r == ESP_OK){ 
    Bme280.set_config_filter(1);
  }
  
  float bme280_temperature{};
  float bme280_pressure{};
  double bme280_humidity{};
  uint8_t bme280_device_id{0};
  uint16_t scd40_co2{0};
  double scd40_temperature{0};
  double scd40_humidity{0};

  if(r == ESP_OK){
    r = Bme280.get_deviceID(&bme280_device_id);
    if(r == ESP_OK){
      ESP_LOGI(MAIN_TAG, "BME280 Device ID: %x", bme280_device_id);
    }
  }
 
  // Initialize the SCD40 I2C device
  if(r == ESP_OK){
    r = Scd40.init(&i2c);
  }
  
  // initialize sd card 
  if(r == ESP_OK){ 
    r = Sd_card.init();
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "fail to initialize SD_CARD compernent.");
    }
  }
  char sd_card_write_data_buffer[400];
  const char file_path[] = "/sensor_log.csv";
  if(r == ESP_OK){
    snprintf(sd_card_write_data_buffer, sizeof(sd_card_write_data_buffer), "YYYY/MM/DD, week, HH:MM:SS, CO2[rpm], Temperature[\u2103], Humidity[%%RH], Pressure[hPa]\n");
    r = Sd_card.write_data(file_path, sizeof(file_path), sd_card_write_data_buffer, 'w');
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "fail to write data to sd_card.");
    }
  }
  vTaskDelay(pdMS_TO_TICKS(50));
  char time_info[100] = "time";

  while(true){
    app.run();

    Bme280.get_all_results(&bme280_temperature, &bme280_humidity, &bme280_pressure);
    if(r == ESP_OK){
     r = Scd40.start_periodic_measurement();
    }
    if(r == ESP_OK){
     vTaskDelay(pdMS_TO_TICKS(5000));
     r = Scd40.get_sensor_data(&scd40_co2, &scd40_temperature, &scd40_humidity);
    }
    if(r == ESP_OK){
     vTaskDelay(pdMS_TO_TICKS(5));
     r = Scd40.stop_periodic_measurement();
    } 
    if(r == ESP_OK){
      r = app.sntp.get_logtime(time_info, sizeof(time_info));
    }
    if(r == ESP_OK){
      snprintf(sd_card_write_data_buffer, sizeof(sd_card_write_data_buffer), "%s, %d, %.2lf, %.2lf, %.2lf\n", time_info, scd40_co2, bme280_temperature, bme280_humidity, bme280_pressure);
      r = Sd_card.write_data(file_path, sizeof(file_path), sd_card_write_data_buffer, 'a');
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
