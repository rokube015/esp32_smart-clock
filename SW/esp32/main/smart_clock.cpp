#include <iostream>
#include <ctime>
#include <algorithm>
#include <cmath>
#include "esp_timer.h"

#include "smart_clock.h"

bool SMART_CLOCK::download_enabled(){
  std::lock_guard<std::mutex> guard(session_mutex);
  return download_window.enabled();
}

esp_err_t SMART_CLOCK::start_data_server(){
  if(data_server != nullptr){ return ESP_OK; }
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 8080;
  config.ctrl_port = 32769; // Provisioning uses the default HTTP/control ports.
  config.max_req_hdr_len = 2048; // Allow normal desktop browser headers.
  config.lru_purge_enable = true;
  esp_err_t result = httpd_start(&data_server, &config);
  if(result != ESP_OK){ return result; }
  result = sensor_http::register_handlers(data_server, http_access);
  if(result != ESP_OK){
    httpd_stop(data_server);
    data_server = nullptr;
  }
  else{
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip {};
    if(netif != nullptr && esp_netif_get_ip_info(netif, &ip) == ESP_OK){
      ESP_LOGI(SMART_CLOCK_TAG, "Sensor data: http://" IPSTR ":8080/", IP2STR(&ip.ip));
    }
    notify_display_update(this);
  }
  return result;
}

void SMART_CLOCK::handle_button_event(const BUTTON::Event& event){
  if(!application_ready.load()){ return; }
  if(event.type == BUTTON::EventType::PRESSED && event.pin == GPIO_NUM_4){
    {
      std::lock_guard<std::mutex> guard(session_mutex);
      download_window.open(xTaskGetTickCount());
    }
    ESP_LOGI(SMART_CLOCK_TAG, "IO4 button pressed; enabling sensor downloads for 5 idle minutes");
    notify_display_update(this);
  }
}

void SMART_CLOCK::network_task_entry(void* arg){
  auto* instance = static_cast<SMART_CLOCK*>(arg);
  xEventGroupWaitBits(instance->start_gate, 1, pdFALSE, pdTRUE, portMAX_DELAY);
  instance->network_task();
}

void SMART_CLOCK::network_task(){
  // Task entry waits on the shared startup gate.
  wifi_run();
  while(true){
    BUTTON::Event event {};
    if(xQueueReceive(button_events, &event, pdMS_TO_TICKS(1000)) == pdTRUE){
      handle_button_event(event);
    }
    wifi_run();
  }
}

SMART_CLOCK::SMART_CLOCK(){
  esp_log_level_set(SMART_CLOCK_TAG, ESP_LOG_INFO);
  ESP_LOGI(SMART_CLOCK_TAG, "set SMART_CLOCK_TAG log level: %d", ESP_LOG_INFO);
}

CLOCK_DISPLAY::Context SMART_CLOCK::display_context(){
  CLOCK_DISPLAY::Context context;
  context.provisioning = wifi.get_state() == WIFI::state_e::PROVISIONING;
  context.connected = wifi.get_state() == WIFI::state_e::CONNECTED;
  context.downloads = download_enabled();
  wifi.get_setup_info(context.setup_ssid, context.provisioning_pop);
  context.time_valid = sntp.is_time_synchronized();
  if(context.time_valid){
    if(sntp.get_time(context.time, sizeof(context.time)) != ESP_OK ||
       sntp.get_daytime(context.day, sizeof(context.day)) != ESP_OK){ context.time_valid = false; }
  }
  if(context.connected){
    auto* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip {};
    if(netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK){
      snprintf(context.address, sizeof(context.address), IPSTR, IP2STR(&ip.ip));
    }
  }
  return context;
}

void SMART_CLOCK::update_display_task(){
  while(true){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    const SensorData data = sensor_snapshot();
    const esp_err_t result = clock_display.render(data, display_context());
    if(result != ESP_OK){ ESP_LOGW(SMART_CLOCK_TAG, "Display update failed: %s", esp_err_to_name(result)); }
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
    SensorData completed;
    const bool time_valid = sntp.is_time_synchronized();
    const std::time_t measured_at = std::time(nullptr);
    {
      std::lock_guard<std::mutex> guard(sensor_mutex);
      sensor_data.complete(co2_ok, measured_co2, bme_ok, measured.temperature,
          measured.humidity, measured.pressure, measured_at, time_valid);
      completed = sensor_data;
    }
    const auto recorded = recorder.record(completed, sd_ready, sd_card);
    if(recorded == SENSOR_RECORDER::Result::FAILED){ ESP_LOGW(SMART_CLOCK_TAG, "Sensor CSV append failed"); }
    ESP_LOGI(SMART_CLOCK_TAG, "Sensor snapshot: CO2=%u ppm, temperature=%.2f C, humidity=%.2f %%, pressure=%.2f hPa; valid=%d/%d sequence=%lu",
        completed.co2, completed.temperature, completed.humidity, completed.pressure,
        completed.co2_valid, completed.bme_valid(), static_cast<unsigned long>(completed.sequence));
    notify_display_update(this);
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(UPDATE_DISPLAY_INTERVAL));
  }
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

void SMART_CLOCK::get_update_display_task_entry_point(void* arg){
  SMART_CLOCK* pinstance = static_cast<SMART_CLOCK*>(arg);
  xEventGroupWaitBits(pinstance->start_gate, 1, pdFALSE, pdTRUE, portMAX_DELAY);
  pinstance->update_display_task();
}

void SMART_CLOCK::get_monitor_sensor_task_entry_point(void* arg){
  SMART_CLOCK* pinstance = static_cast<SMART_CLOCK*>(arg);
  xEventGroupWaitBits(pinstance->start_gate, 1, pdFALSE, pdTRUE, portMAX_DELAY);
  pinstance->monitor_sensor_task();
}

void SMART_CLOCK::wifi_connected_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
  if(event_base != IP_EVENT || event_id != IP_EVENT_STA_GOT_IP){ return; }
  auto* instance = static_cast<SMART_CLOCK*>(arg);
  instance->ip_received.store(true);
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

  // The SMART_CLOCK network task serializes shutdown with button events and HTTP sessions.
  instance->time_sync_stop_requested.store(true);
  notify_display_update(instance);
}

void SMART_CLOCK::notify_display_update(void* arg){
  auto* instance = static_cast<SMART_CLOCK*>(arg);
  std::lock_guard<std::mutex> guard(instance->notification_mutex);
  if(!instance->application_ready.load()){ return; }
  const TaskHandle_t handle = instance->update_display_handle.load();
  if(handle != nullptr){ xTaskNotifyGive(handle); }
}

esp_err_t SMART_CLOCK::init(){
  if(initialized || application_ready.load()){ return ESP_ERR_INVALID_STATE; }
  esp_err_t r = esp_event_loop_create_default();
  event_loop_owned = r == ESP_OK;
  if(r == ESP_ERR_INVALID_STATE){ r = ESP_OK; }
  if(r == ESP_OK){ r = nvs_flash_init(); nvs_ready = r == ESP_OK; }
  if(r == ESP_OK){ r = clock_display.init(); }
  if(r == ESP_OK){ r = i2c.init(); }
  if(r == ESP_OK){ r = bme280.init(&i2c); }
  if(r == ESP_OK){ r = scd40.init(&i2c); }
  if(r == ESP_OK){ r = bme280.prepare_queue(); }
  if(r == ESP_OK){ r = scd40.prepare_queue(); }
  if(r == ESP_OK){
    co2_buffer = scd40.get_co2_buffer_handle();
    bme280_results_buffer = bme280.get_results_buffer();
    if(sd_card.init() == ESP_OK){ sd_ready = recorder.prepare(sd_card) == ESP_OK; }
    if(!sd_ready){ ESP_LOGW(SMART_CLOCK_TAG, "SD unavailable; measurement, display and latest API remain enabled"); }
    start_gate = xEventGroupCreate();
    button_events = xQueueCreate(8, sizeof(BUTTON::Event));
    if(start_gate == nullptr || button_events == nullptr){ r = ESP_ERR_NO_MEM; }
  }
  if(r == ESP_OK){ r = button.init(GPIO_NUM_4, button_events); }
  if(r == ESP_OK){ r = wifi.init(); }
  if(r == ESP_OK){
    r = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_connected_event_handler, this);
    ip_handler_registered = r == ESP_OK;
  }
  if(r == ESP_OK){
    sntp.set_time_sync_callback(sntp_time_sync_complete, this);
    initialized = true;
  }
  else{ cleanup(); }
  return r;
}

void SMART_CLOCK::cleanup(){
  // Used only before releasing the startup gate: deleted tasks cannot own driver locks.
  {
    std::lock_guard<std::mutex> guard(notification_mutex);
    application_ready.store(false);
    const auto display = update_display_handle.exchange(nullptr);
    if(display){ vTaskDelete(display); }
  }
  sntp.set_time_sync_callback(nullptr, nullptr);
  if(ip_handler_registered){
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_connected_event_handler);
    ip_handler_registered = false;
  }
  if(network_task_handle){ vTaskDelete(network_task_handle); network_task_handle = nullptr; }
  if(sensor_task_handle){ vTaskDelete(sensor_task_handle); sensor_task_handle = nullptr; }
  button.deinit();
  scd40.deinit(); bme280.deinit();
  co2_buffer = nullptr; bme280_results_buffer = nullptr;
  if(button_events){ vQueueDelete(button_events); button_events = nullptr; }
  if(start_gate){ vEventGroupDelete(start_gate); start_gate = nullptr; }
  sntp.stop(); wifi.deinit();
  sd_card.deinit(); sd_ready = false;
  i2c.deinit(); clock_display.deinit();
  if(nvs_ready){ nvs_flash_deinit(); nvs_ready = false; }
  if(event_loop_owned){ esp_event_loop_delete_default(); event_loop_owned = false; }
  initialized = false;
}

void SMART_CLOCK::wifi_run(){
  wifi.process_events();
  const int64_t now = esp_timer_get_time();
  const auto state = wifi.get_state();
  if(state != wifi_state){ wifi_state = state; notify_display_update(this); }
  bool access;
  {
    std::lock_guard<std::mutex> guard(session_mutex);
    download_window.expire(xTaskGetTickCount());
    access = download_window.enabled();
  }
  if(time_sync_stop_requested.exchange(false)){
    sync_schedule.success();
    sync_started = false;
    sntp.stop();
  }
  bool date_changed = false;
  if(sntp.is_time_synchronized()){
    const std::time_t wall_now = std::time(nullptr);
    std::tm local {};
    if(localtime_r(&wall_now, &local)){
      date_changed = local.tm_year != last_sync_year.load() || local.tm_yday != last_sync_yday.load();
    }
  }
  if(sync_schedule.due(now, date_changed)){ sync_schedule.begin(now); sync_started = false; }
  if(sync_schedule.expire(now, state == WIFI::state_e::PROVISIONING)){
    sntp.stop(); sync_started = false;
    ESP_LOGW(SMART_CLOCK_TAG, "Time sync timed out; retry in 30 minutes");
  }
  if(!access && data_server){ httpd_stop(data_server); data_server = nullptr; notify_display_update(this); }
  const bool network_needed = access || sync_schedule.active();
  if(!network_needed && state != WIFI::state_e::PROVISIONING){
    if(state != WIFI::state_e::INITIALIZED && state != WIFI::state_e::NOT_INITIALIZED){
      const esp_err_t result = wifi.stop();
      if(result != ESP_OK){ ESP_LOGW(SMART_CLOCK_TAG, "Wi-Fi stop failed: %s", esp_err_to_name(result)); }
      notify_display_update(this);
    }
    return;
  }
  if(network_needed && state == WIFI::state_e::INITIALIZED){
    const esp_err_t result = wifi.start();
    if(result != ESP_OK){ ESP_LOGW(SMART_CLOCK_TAG, "Wi-Fi start failed: %s", esp_err_to_name(result)); }
    return;
  }
  if(state == WIFI::state_e::READY_TO_CONNECT || state == WIFI::state_e::DISCONNECTED || state == WIFI::state_e::ERROR){ wifi.begin(); }
  const bool connected_event = ip_received.exchange(false);
  if(state == WIFI::state_e::CONNECTED){
    if(sync_schedule.active() && (!sync_started || connected_event)){
      if(!sntp.is_running()){ sync_started = sntp.init() == ESP_OK; }
      else{ sync_started = sntp.set_update_interval(60 * 60 * 1000, true); }
    }
    if(access){
      const esp_err_t result = start_data_server();
      if(result != ESP_OK){ ESP_LOGW(SMART_CLOCK_TAG, "HTTP server start failed: %s", esp_err_to_name(result)); }
    }
  }
}

esp_err_t SMART_CLOCK::run(){
  if(!initialized || application_ready.load()){ return ESP_ERR_INVALID_STATE; }
  esp_err_t r = scd40.create_task("measure_co2", 2048, 10, start_gate);
  if(r == ESP_OK){ r = bme280.create_task("measure_bme280", 2048, 10, start_gate); }
  if(r == ESP_OK){ r = create_monitor_sensor_task("monitor_sensor", 4096, 10); }
  if(r == ESP_OK){ r = create_update_display_task("update_display", 4096, 10); }
  if(r == ESP_OK && xTaskCreate(network_task_entry, "clock_network", 6144, this, 5, &network_task_handle) != pdPASS){ r = ESP_ERR_NO_MEM; }
  if(r == ESP_OK){ r = button.create_task("monitor_button", 2048, 5, start_gate); }
  if(r != ESP_OK){ cleanup(); return r; }
  application_ready.store(true);
  // A notification remains pending even if the display task has not reached its wait yet.
  notify_display_update(this);
  xEventGroupSetBits(start_gate, 1);
  return ESP_OK;
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

SensorData SMART_CLOCK::http_snapshot(void* arg){ return static_cast<SMART_CLOCK*>(arg)->sensor_snapshot(); }
bool SMART_CLOCK::http_begin(void* arg){
  auto& clock = *static_cast<SMART_CLOCK*>(arg);
  std::lock_guard<std::mutex> guard(clock.session_mutex);
  return clock.download_window.begin_request(xTaskGetTickCount());
}
void SMART_CLOCK::http_end(void* arg){
  auto& clock = *static_cast<SMART_CLOCK*>(arg);
  std::lock_guard<std::mutex> guard(clock.session_mutex);
  clock.download_window.end_request(xTaskGetTickCount());
}
bool SMART_CLOCK::http_sd_available(void* arg){ return static_cast<SMART_CLOCK*>(arg)->sd_ready; }
