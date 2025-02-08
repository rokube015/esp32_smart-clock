#include <iostream>

#include "smart_clock.h"
#include "wifi_pass.h"

LGFX_Sprite SMART_CLOCK::black_sprite;

SMART_CLOCK::SMART_CLOCK(){
  esp_log_level_set(SMART_CLOCK_TAG, ESP_LOG_INFO);
  ESP_LOGI(SMART_CLOCK_TAG, "set SMART_CLOCK_TAG log level: %d", ESP_LOG_INFO);
}

void SMART_CLOCK::update_display_timer_task(){
  ESP_LOGI(SMART_CLOCK_TAG, "execute update_display_timer_task");
  vTaskNotifyGiveFromISR(update_display_handle, NULL);
}

void SMART_CLOCK::update_display_task(){
  char timestamp[100] = "time";
  char day_info[50] = "time";
  char time_info[50] = "time";
  char sd_card_write_data_buffer[400];

  while(1){
    esp_err_t r = ESP_OK;
    esp_err_t r2 = ESP_OK;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ESP_LOGI(SMART_CLOCK_TAG, "execute update_display_task");
    if(r == ESP_OK){
      r = sntp.get_logtime(timestamp, sizeof(timestamp));
    }
    if(r == ESP_OK){
      r = sntp.get_time(time_info, sizeof(time_info));
    }
    if(r == ESP_OK){
      r = sntp.get_daytime(day_info, sizeof(day_info));
    }
    if(r == ESP_OK){
      snprintf(sd_card_write_data_buffer, sizeof(sd_card_write_data_buffer), "%s, %d, %.2lf, %.2lf, %.2lf\n", timestamp, co2, temperature, humidity, pressure);
      r2 = sd_card.write_data(file_path, sizeof(file_path), sd_card_write_data_buffer, 'a');
      if(r != ESP_OK){
        ESP_LOGE(SMART_CLOCK_TAG, "fail to write sensor log to sd_card.");
      }
    }
    if(r == ESP_OK){
      std::cout << "==================================================" << std::endl;
      std::cout << "Time              : " << timestamp << std::endl;
      std::cout << "BME280 Temperature: " << temperature << "\u2103" << std::endl;
      std::cout << "BME280 Humidity   : " << humidity << "%" << std::endl;
      std::cout << "BME280 Pressure   : " << pressure << "hPa" << std::endl;
      std::cout << "SCD40  CO2        : " << co2 << "ppm" << std::endl;
      std::cout << "==================================================" << std::endl;;
    }
    if(r == ESP_OK){
      r = display_epaper(day_info, time_info);
    }
  }
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

esp_err_t SMART_CLOCK::display_epaper(char* pday_info, char* ptime_info){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){ 
    char display_buffer[50] = "\0";
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t x1 = 0;
    uint16_t y1 = 0;

    black_sprite.fillScreen(WHITE);
    black_sprite.setCursor(0, 0);
    black_sprite.setTextSize(2);
    black_sprite.setFont(&fonts::Font8);
    x = black_sprite.width()/2 - black_sprite.textWidth(ptime_info)/2;
    y = 10;
    black_sprite.setCursor(x, y);
    black_sprite.printf("%s\n", ptime_info);
    black_sprite.setFont(&fonts::FreeSans24pt7b);
    black_sprite.setTextSize(1);
    x = black_sprite.width()/2 - black_sprite.textWidth(pday_info)/2;
    y = black_sprite.getCursorY() + 30;
    black_sprite.setCursor(x, y);
    black_sprite.printf("%s\n", pday_info);
    black_sprite.setTextSize(1.5);
    snprintf(display_buffer, sizeof(display_buffer), "CO2  %dppm\n", co2);
    x = black_sprite.width()/2 - black_sprite.textWidth(display_buffer)/2;
    y = black_sprite.getCursorY();
    y += 30; 
    black_sprite.setCursor(x, y);
    black_sprite.printf(display_buffer);
    black_sprite.setTextSize(1);
    x = black_sprite.getCursorX();
    y = black_sprite.getCursorY() + 10;
    black_sprite.printf("Temperature");
    x1 =x;
    x = black_sprite.getCursorX();
    black_sprite.setCursor(x1, y + black_sprite.fontHeight());
    black_sprite.printf("%.1lf", temperature);
    black_sprite.setFont(&fonts::lgfxJapanGothicP_24);
    black_sprite.setTextSize(2);
    black_sprite.printf("\u2103");
    black_sprite.setFont(&fonts::FreeSans24pt7b);
    black_sprite.setTextSize(1);
    x += 50; 
    black_sprite.setCursor(x, y);
    black_sprite.printf("Humidity");
    x1 = x; 
    x = black_sprite.getCursorX();
    black_sprite.setCursor(x1, y + black_sprite.fontHeight());
    black_sprite.printf("%.2lf%%\n", humidity);
    x += 50; 
    black_sprite.setCursor(x, y);
    black_sprite.printf("Pressure");
    x1 = x; 
    x = black_sprite.getCursorX();
    black_sprite.setCursor(x1, y + black_sprite.fontHeight());
    black_sprite.printf("%.2lfhpa\n", pressure);
  }
  if(r == ESP_OK){
    if(e_paper.get_state() == EPAPER4IN26::state_e::SLEEP){
      r = e_paper.init_epaper();
    }
    r |= e_paper.display((uint8_t*)black_sprite.getBuffer(), e_paper.get_display_bytes());
    r |= e_paper.set_sleep_mode();
  }
  return r;
}

esp_err_t SMART_CLOCK::create_update_display_timer_task(const char* pname){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){ 
    update_display_timer_handle = xTimerCreate(pname, pdMS_TO_TICKS(60000), pdTRUE, this, get_update_display_timer_task_entry_point);
    if(update_display_timer_handle == NULL){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to create update_display_timer_task.");
      r = ESP_FAIL;
    }
  }
  return r;
}

esp_err_t SMART_CLOCK::create_update_display_task(const char* pname, uint16_t stack_size, UBaseType_t task_priority){
  esp_err_t r = ESP_OK;
  BaseType_t r2 = pdTRUE;
  if(r == ESP_OK){ 
    r2 = xTaskCreate(get_update_display_task_entry_point, pname, stack_size, this, task_priority, &update_display_handle);
    if(r2 != pdTRUE){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to create update_display_handle.");
      r = ESP_FAIL;
    }
  }
  return r;
}

void SMART_CLOCK::get_update_display_timer_task_entry_point(TimerHandle_t timer_handle){
  SMART_CLOCK* pinstance = static_cast<SMART_CLOCK*>(pvTimerGetTimerID(timer_handle));
  pinstance->update_display_timer_task();
}

void SMART_CLOCK::get_update_display_task_entry_point(void* arg){
  SMART_CLOCK* pinstance = static_cast<SMART_CLOCK*>(arg);
  pinstance->update_display_task();
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
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
  if(r == ESP_OK){
    black_sprite.setColorDepth(1);
    black_sprite.createSprite(e_paper.get_display_resolution_width(), e_paper.get_display_resolution_height());
    black_sprite.setTextWrap(true);
    black_sprite.fillScreen(WHITE);
    black_sprite.setCursor(0, 0);
    black_sprite.setFont(&fonts::Font4);
    black_sprite.setTextColor(BLACK);
    black_sprite.setTextSize(2);
    black_sprite.print("smart clock initializing...");
    r = e_paper.display((uint8_t*)black_sprite.getBuffer(), e_paper.get_display_bytes());
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
  if(r == ESP_OK){
    r = create_update_display_task("update_display", 4096, 10);
  }
  if(r == ESP_OK){
    r = create_update_display_timer_task("update_diplay_timer");
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

esp_err_t SMART_CLOCK::run(void){
  esp_err_t r = ESP_OK; 
  BaseType_t r2 = pdPASS;
  if(r == ESP_OK){
    if(update_display_timer_handle != NULL){
      r2 = xTimerStart(update_display_timer_handle, 0);
      r = (r2 == pdPASS ? ESP_OK : ESP_FAIL);
      if(r != ESP_OK){
        ESP_LOGE(SMART_CLOCK_TAG, "fail to start update_display_timer task");
      }
    }
  }
  return r;
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
