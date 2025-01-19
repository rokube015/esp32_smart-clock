#include <iostream>

#include "smart_clock.h"
#include "wifi_pass.h"

LGFX_Sprite SMART_CLOCK::black_sprite;
LGFX_Sprite SMART_CLOCK::red_sprite;

SMART_CLOCK::SMART_CLOCK(){
  esp_log_level_set(SMART_CLOCK_TAG, ESP_LOG_INFO);
  ESP_LOGI(SMART_CLOCK_TAG, "set SMART_CLOCK_TAG log level: %d", ESP_LOG_INFO);
}

void SMART_CLOCK::monitor_sensor_task(){
  BaseType_t r2 = pdTRUE;

  while(true){
    scd40.notify_measurement_start();
    bme280.notify_measurement_start();
    r2 = xQueueReceive(co2_buffer, &co2, pdMS_TO_TICKS(300000));
    if(r2 != pdTRUE){
      ESP_LOGW(SMART_CLOCK_TAG, "fail to receive data from co2 buffer");
    }
    r2 = xQueueReceive(bme280_results_buffer, &results_data, pdMS_TO_TICKS(300000));
    if(r2 == pdTRUE){
      temperature = results_data.temperature;
      pressure = results_data.pressure;
      humidity = results_data.humidity;
    }
    else{
      ESP_LOGW(SMART_CLOCK_TAG, "fail to receive data from bme280 buffer.");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
  vTaskDelete(NULL);
}

void SMART_CLOCK::get_monitor_sensor_task_entry_point(void* arg){
  SMART_CLOCK* pinstance = static_cast<SMART_CLOCK*>(arg);
  pinstance->monitor_sensor_task();
}

void SMART_CLOCK::init(void){
  esp_err_t r = ESP_OK;
  esp_event_loop_create_default();
  nvs_flash_init();

  // initialize e-paper
  if(r == ESP_OK){
    r = e_paper.init();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  if(r == ESP_OK){
    r = e_paper.clear_screen();
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to clear display.");
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
  if(r == ESP_OK){
    black_sprite.setColorDepth(1);
    black_sprite.createSprite(e_paper.get_display_resolution_width(), e_paper.get_display_resolution_height());
    black_sprite.setTextWrap(true);
    black_sprite.fillScreen(WHITE);
    black_sprite.setCursor(0, 0);
    black_sprite.setFont(&fonts::Font4);
    black_sprite.setTextColor(BLACK);
    black_sprite.setTextSize(1);
    
    red_sprite.setColorDepth(1);
    red_sprite.createSprite(e_paper.get_display_resolution_width(), e_paper.get_display_resolution_height());
    red_sprite.setTextWrap(false);
    red_sprite.fillScreen(WHITE);
    red_sprite.setCursor(0, 0);
    red_sprite.setFont(&fonts::Font0);
    red_sprite.setTextColor(BLACK);
    red_sprite.setTextSize(1);
    
    black_sprite.print("hello world!");
    red_sprite.fillRect(50, 200, 150, 160, BLACK);
    ESP_LOG_BUFFER_HEX_LEVEL(SMART_CLOCK_TAG, black_sprite.getBuffer(), e_paper.get_display_bytes(), ESP_LOG_INFO);
    ESP_LOG_BUFFER_HEX_LEVEL(SMART_CLOCK_TAG, red_sprite.getBuffer(), e_paper.get_display_bytes(), ESP_LOG_INFO);
    r = e_paper.display((uint8_t*)black_sprite.getBuffer(), 
        e_paper.get_display_bytes(),
        (uint8_t*)red_sprite.getBuffer(), 
        e_paper.get_display_bytes());
    ESP_LOGI(SMART_CLOCK_TAG, "print hello world.");
    /*
    vTaskDelay(pdMS_TO_TICKS(10000));
    red_sprite.fillRect(0, 200, 200, 160, BLACK);
    r = e_paper.display((uint8_t*)black_sprite.getBuffer(), 
        e_paper.get_display_bytes(),
        (uint8_t*)red_sprite.getBuffer(), 
        e_paper.get_display_bytes());
    ESP_LOGI(SMART_CLOCK_TAG, "print hello world.");
    */
  }
 
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
  vTaskDelay(pdMS_TO_TICKS(3000));
  if(r == ESP_OK){
    r = scd40.create_task("measure_co2", 2048, 10);
  }
  if(r == ESP_OK){
    r = bme280.create_task("measure_bme280", 2048, 10);
  }
  if(r == ESP_OK){
    r = create_monitor_sensor_task("monitor_sensor", 2048, 10);
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to create monitor_sensor task");
    }
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

esp_err_t SMART_CLOCK::create_monitor_sensor_task(const char* pname, uint16_t stack_size, UBaseType_t task_priority){
  esp_err_t r = ESP_OK;
  BaseType_t r2 = pdTRUE;
  co2_buffer = scd40.get_co2_buffer_handle();
  bme280_results_buffer = bme280.get_results_buffer(); 
  if(r == ESP_OK){ 
    r2 = xTaskCreate(get_monitor_sensor_task_entry_point, pname, stack_size, this, task_priority, &sensor_task_handle);
    if(r2 != pdTRUE){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to create monitor_sensor_data_task.");
      r = ESP_FAIL;
    }
  }
  return r;
}
