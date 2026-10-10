#include <iostream>
#include <ctime>

#include "smart_clock.h"

LGFX_Sprite SMART_CLOCK::black_sprite;

SMART_CLOCK::SMART_CLOCK(){
  esp_log_level_set(SMART_CLOCK_TAG, ESP_LOG_INFO);
  ESP_LOGI(SMART_CLOCK_TAG, "set SMART_CLOCK_TAG log level: %d", ESP_LOG_INFO);
}

void SMART_CLOCK::update_display_timer_task(){
  ESP_LOGI(SMART_CLOCK_TAG, "execute update_display_timer_task");
  notify_display_update(this);
}

void SMART_CLOCK::update_display_task(){
  char timestamp[100] = "time";
  char sd_card_write_data_buffer[400];

  while(1){
    esp_err_t r = ESP_OK;
    esp_err_t r2 = ESP_OK;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ESP_LOGI(SMART_CLOCK_TAG, "execute update_display_task");
    const SensorData data = sensor_snapshot();
    const bool time_synchronized = sntp.is_time_synchronized();
    if(r == ESP_OK && time_synchronized){
      r = sntp.get_logtime(timestamp, sizeof(timestamp));
      if(r != ESP_OK){
        ESP_LOGE(SMART_CLOCK_TAG, "fail to get timestamp.");
      }
    }
    
    if(r == ESP_OK && time_synchronized && sd_ready && data.valid){
      snprintf(sd_card_write_data_buffer, sizeof(sd_card_write_data_buffer), "%s, %d, %.2lf, %.2lf, %.2lf\n", timestamp, data.co2, data.temperature, data.humidity, data.pressure);
      r2 = sd_card.write_data(file_path, sizeof(file_path), sd_card_write_data_buffer, 'a');
      if(r2 != ESP_OK){
        ESP_LOGE(SMART_CLOCK_TAG, "fail to write sensor log to sd_card.");
      }
    }
    if(r == ESP_OK){
      r = display_epaper(data);
      if(r != ESP_OK){
        ESP_LOGE(SMART_CLOCK_TAG, "fail to display epaper.");
      }
    }
  }
}

void SMART_CLOCK::monitor_sensor_task(){
  TickType_t last_wake_time = xTaskGetTickCount();
  while(true){
    // Discard a late result from an earlier failed request.
    xQueueReset(co2_buffer);
    xQueueReset(bme280_results_buffer);
    scd40.notify_measurement_start();
    bme280.notify_measurement_start();
    uint16_t measured_co2 = 0;
    BME280::results_data_t measured {};
    const bool co2_ok = xQueueReceive(co2_buffer, &measured_co2, pdMS_TO_TICKS(15000)) == pdTRUE;
    const bool bme_ok = xQueueReceive(bme280_results_buffer, &measured, pdMS_TO_TICKS(5000)) == pdTRUE;
    if(co2_ok && bme_ok){
      std::lock_guard<std::mutex> guard(sensor_mutex);
      sensor_data = {measured_co2, measured.temperature, measured.humidity, measured.pressure, true};
      ESP_LOGI(SMART_CLOCK_TAG, "Sensor snapshot: CO2=%u ppm, temperature=%.2f C, humidity=%.2f %%, pressure=%.2f hPa",
               sensor_data.co2, sensor_data.temperature, sensor_data.humidity, sensor_data.pressure);
    }
    else{
      ESP_LOGW(SMART_CLOCK_TAG, "Sensor request failed (CO2=%d, BME280=%d); keeping previous snapshot", co2_ok, bme_ok);
      std::lock_guard<std::mutex> guard(sensor_mutex);
      sensor_data.valid = false;
    }
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(UPDATE_DISPLAY_INTERVAL));
  }
}

esp_err_t SMART_CLOCK::display_epaper(const SensorData& data){
  if(!display_ready){ return ESP_ERR_INVALID_STATE; }
  const uint16_t co2 = data.co2;
  const float temperature = data.temperature;
  const double humidity = data.humidity;
  const float pressure = data.pressure;
  esp_err_t r = ESP_OK;
  if(wifi.get_state() == WIFI::state_e::PROVISIONING){
    char setup_ssid[33] {};
    char provisioning_pop[17] {};
    wifi.get_setup_info(setup_ssid, provisioning_pop);
    black_sprite.fillScreen(WHITE);
    black_sprite.setTextWrap(false);
    black_sprite.setTextColor(BLACK);
    black_sprite.setFont(&fonts::Font4);
    black_sprite.setTextSize(2);
    black_sprite.setCursor(32, 24);
    black_sprite.println("Wi-Fi setup");
    black_sprite.setTextSize(1);
    black_sprite.setCursor(32, 88);
    black_sprite.println("Connect your phone to this setup network:");
    black_sprite.setCursor(32, 122);
    black_sprite.printf("SSID: %s", setup_ssid);
    black_sprite.setCursor(32, 156);
    black_sprite.println("AP password: none (open network)");
    black_sprite.setCursor(32, 205);
    black_sprite.println("In Espressif Provisioning, select SoftAP.");
    black_sprite.setCursor(32, 239);
    black_sprite.println("Enter this proof of possession (PoP):");
    black_sprite.setTextSize(3);
    black_sprite.setCursor(32, 278);
    black_sprite.println(provisioning_pop);
    black_sprite.setTextSize(1);
    black_sprite.setCursor(32, 385);
    black_sprite.println("Then enter your 2.4 GHz Wi-Fi SSID and password.");
  }
  else if(!sntp.is_time_synchronized()){
    black_sprite.fillScreen(WHITE);
    black_sprite.setTextWrap(false);
    black_sprite.setTextColor(BLACK);
    black_sprite.setFont(&fonts::Font4);
    black_sprite.setTextSize(2);
    black_sprite.setCursor(32, 160);
    black_sprite.println("Wi-Fi connected");
    black_sprite.setTextSize(1);
    black_sprite.setCursor(32, 220);
    black_sprite.println("Synchronizing time...");
  }
  else{
    char day_info[50] = "\0";
    char time_info[50] = "\0";

    if(r == ESP_OK){
      r = sntp.get_time(time_info, sizeof(time_info));
    }
    if(r == ESP_OK){
      r = sntp.get_daytime(day_info, sizeof(day_info));
    }

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
    x = black_sprite.width()/2 - black_sprite.textWidth(time_info)/2;
    y = 10;
    black_sprite.setCursor(x, y);
    black_sprite.printf("%s\n", time_info);
    black_sprite.setFont(&fonts::FreeSans24pt7b);
    black_sprite.setTextSize(1);
    x = black_sprite.width()/2 - black_sprite.textWidth(day_info)/2;
    y = black_sprite.getCursorY() + 30;
    black_sprite.setCursor(x, y);
    black_sprite.printf("%s\n", day_info);
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
  }

  if(r == ESP_OK){
    if(e_paper.get_state() == EPAPER4IN26::state_e::SLEEP ||
       e_paper.get_state() == EPAPER4IN26::state_e::NOT_INITIALIZED){
      r = e_paper.init_epaper();
    }
    if(r == ESP_OK){ r = e_paper.display((uint8_t*)black_sprite.getBuffer(), e_paper.get_display_bytes()); }
    if(r == ESP_OK){ r = e_paper.set_sleep_mode(); }
  }
  return r;
}

esp_err_t SMART_CLOCK::create_update_display_timer_task(const char* pname){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){ 
    update_display_timer_handle = xTimerCreate(pname, pdMS_TO_TICKS(UPDATE_DISPLAY_INTERVAL), pdTRUE, this, get_update_display_timer_task_entry_point);
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
    TaskHandle_t handle = nullptr;
    r2 = xTaskCreate(get_update_display_task_entry_point, pname, stack_size, this, task_priority, &handle);
    if(r2 == pdTRUE){ update_display_handle.store(handle); }
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

void SMART_CLOCK::wifi_connected_event_handler(void* arg, esp_event_base_t event_base,
                                               int32_t event_id, void* event_data){
  if(event_base != IP_EVENT || event_id != IP_EVENT_STA_GOT_IP){
    return;
  }

  SMART_CLOCK* instance = static_cast<SMART_CLOCK*>(arg);
  if(!instance->application_ready.load()){ return; }
  ESP_LOGI(SMART_CLOCK_TAG, "Wi-Fi connected; requesting immediate display update");
  if(!instance->sntp.is_running()){
    instance->sntp.init();
  }
  else{
    instance->sntp.set_update_interval(60 * 60 * 1000, true);
  }
  notify_display_update(instance);
}

void SMART_CLOCK::sntp_time_sync_complete(void* arg){
  SMART_CLOCK* instance = static_cast<SMART_CLOCK*>(arg);
  const std::time_t now = std::time(nullptr);
  std::tm local_time{};
  if(localtime_r(&now, &local_time) != nullptr){
    instance->last_sync_year.store(local_time.tm_year);
    instance->last_sync_yday.store(local_time.tm_yday);
  }
  instance->daily_wifi_sync_requested.store(false);

  const esp_err_t stop_result = instance->wifi.stop();
  if(stop_result == ESP_OK){
    ESP_LOGI(SMART_CLOCK_TAG, "NTP synchronized; Wi-Fi stopped for power saving");
  }
  else{
    ESP_LOGW(SMART_CLOCK_TAG, "Failed to stop Wi-Fi after NTP sync: %s",
             esp_err_to_name(stop_result));
  }
  notify_display_update(instance);
}

void SMART_CLOCK::notify_display_update(void* arg){
  SMART_CLOCK* instance = static_cast<SMART_CLOCK*>(arg);
  if(!instance->application_ready.load()){ return; }
  const TaskHandle_t handle = instance->update_display_handle.load();
  if(handle != nullptr){ xTaskNotifyGive(handle); }
}

esp_err_t SMART_CLOCK::init(void){
  esp_err_t r = ESP_OK;
  r = esp_event_loop_create_default();
  if(r != ESP_OK && r != ESP_ERR_INVALID_STATE){ return r; }
  r = nvs_flash_init();
  if(r != ESP_OK){
    ESP_LOGE(SMART_CLOCK_TAG, "NVS initialization failed: %s", esp_err_to_name(r));
    return r;
  }

  // initialize e-paper
  if(r == ESP_OK){
    const esp_err_t panel_result = e_paper.init();
    display_ready = panel_result == ESP_OK || panel_result == ESP_ERR_TIMEOUT;
    r = ESP_OK;
    if(panel_result != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to initialize e-paper.");
    }
  }
  if(r == ESP_OK && display_ready){
    const esp_err_t clear_result = e_paper.clear_screen();
    if(clear_result != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to clear display.");
    }
  }
  if(r == ESP_OK){
    black_sprite.setColorDepth(1);
    if(black_sprite.createSprite(e_paper.get_display_resolution_width(), e_paper.get_display_resolution_height()) == nullptr){
      ESP_LOGE(SMART_CLOCK_TAG, "Failed to allocate display buffer");
      return ESP_ERR_NO_MEM;
    }
    black_sprite.setTextWrap(true);
    black_sprite.fillScreen(WHITE);
    black_sprite.setCursor(0, 0);
    black_sprite.setFont(&fonts::Font4);
    black_sprite.setTextColor(BLACK);
    black_sprite.setTextSize(2);
    black_sprite.print("smart clock initializing...");
    if(display_ready){ r = e_paper.display((uint8_t*)black_sprite.getBuffer(), e_paper.get_display_bytes()); }
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to display initializing message.");
      r = ESP_OK;
    } 
  }
  if(r == ESP_OK){
    sntp.set_time_sync_callback(&sntp_time_sync_complete, this);
    r = wifi.init();
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to initialize wifi component.");
    }
  }
  if(r == ESP_OK){
    r = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                   &wifi_connected_event_handler, this);
  }
  if(r == ESP_OK && display_ready && wifi.get_state() == WIFI::state_e::PROVISIONING){
    r = display_epaper(sensor_snapshot());
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to display Wi-Fi setup information.");
      r = ESP_OK;
    }
  }
  // Initialize the I2C
  if(r == ESP_OK){ 
    r = i2c.init();
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to initialize i2c.");
    }
  }
  // Initialize the BME280 I2C device
  if(r == ESP_OK){ 
    r = bme280.init(&i2c);
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to initialize bme280.");
    }
  }
  // Initialize the SCD40 I2C device
  if(r == ESP_OK){
    r = scd40.init(&i2c);
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to initialize scd40.");
    }
  }
  if(r == ESP_OK){
    r = scd40.create_task("measure_co2", 2048, 10);
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to create scd40 measure_co2 task");
    }
  }
  if(r == ESP_OK){
    r = bme280.create_task("measure_bme280", 2048, 10);
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to create bme280 measure_bme280 task");
    }
  }
  if(r == ESP_OK){
    r = create_monitor_sensor_task("monitor_sensor", 2048, 10);
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to create monitor_sensor task");
    }
  }
  // initialize sd card 
  esp_err_t r2 = ESP_OK;
  if(r == ESP_OK){ 
    r2 = sd_card.init();
    if(r2 != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to initialize sd_card component.");
    }
  }
  if(r == ESP_OK && r2 == ESP_OK){
    char sd_card_write_data_buffer[400];
    snprintf(sd_card_write_data_buffer, sizeof(sd_card_write_data_buffer), "YYYY/MM/DD, week, HH:MM:SS, CO2[ppm], Temperature[degree], Humidity[%%], Pressure[hPa]\n");
    r2 = sd_card.write_data(file_path, sizeof(file_path), sd_card_write_data_buffer, 'h');
    sd_ready = (r2 == ESP_OK);
    if(r2 != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to write data to sd_card.");
    }
  }
  if(r == ESP_OK){
    r = create_update_display_task("update_display", 4096, 10);
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to create update_display task");
    }
  }
  if(r == ESP_OK){
    r = create_update_display_timer_task("update_diplay_timer");
    if(r != ESP_OK){
      ESP_LOGE(SMART_CLOCK_TAG, "fail to create update_display_timer task");
    }
  }
  application_ready.store(r == ESP_OK);
  return r;
}

void SMART_CLOCK::wifi_run(void){
  wifi_state = wifi.get_state();
  bool is_sntp_initialized = sntp.is_running();

  if(wifi_state == WIFI::state_e::INITIALIZED && sntp.is_time_synchronized()){
    const std::time_t now = std::time(nullptr);
    std::tm local_time{};
    if(localtime_r(&now, &local_time) != nullptr &&
       (local_time.tm_year != last_sync_year.load() ||
        local_time.tm_yday != last_sync_yday.load()) &&
       !daily_wifi_sync_requested.exchange(true)){
      const esp_err_t start_result = wifi.start();
      if(start_result == ESP_OK){
        ESP_LOGI(SMART_CLOCK_TAG, "Date changed; Wi-Fi started for daily time sync");
      }
      else{
        daily_wifi_sync_requested.store(false);
        ESP_LOGW(SMART_CLOCK_TAG, "Failed to start Wi-Fi for daily time sync: %s",
                 esp_err_to_name(start_result));
      }
    }
  }

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
      wifi.begin();
      break;
    case WIFI::state_e::CONNECTED:
      ESP_LOGI(SMART_CLOCK_TAG, "wifi status: CONNECTED");
      if(!is_sntp_initialized){
       sntp.init();
      } 
      break;
    case WIFI::state_e::PROVISIONING:
      ESP_LOGI(SMART_CLOCK_TAG, "wifi status: PROVISIONING");
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
