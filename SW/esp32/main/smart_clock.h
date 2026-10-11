#pragma once

#include <atomic>
#include <mutex>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "i2c_base.h"
#include "bme280.h"
#include "scd40.h"
#include "e_paper.h"
#include "sd_card.h"
#include "sntp_interface.h"
#include "button.h"
#include "esp_http_server.h"
#include "button_wifi_session.h"

#include "clock_display.h"
#include "sensor_recorder.h"
#include "sensor_http.h"
#include "freertos/event_groups.h"
#include "sync_schedule.h"

class SMART_CLOCK final{
  private:
    constexpr static const char* SMART_CLOCK_TAG = "smart_clock"; 
  
    constexpr static uint16_t UPDATE_DISPLAY_INTERVAL {60 * 1000}; //[ms]  
    SensorData sensor_data;
    std::mutex sensor_mutex;
    bool sd_ready {false};
    CLOCK_DISPLAY clock_display;
    SENSOR_RECORDER recorder;
    EventGroupHandle_t start_gate {nullptr};
    bool initialized {false};
    bool event_loop_owned {false};
    bool nvs_ready {false};
    bool ip_handler_registered {false};
    std::mutex notification_mutex;
    std::atomic<bool> ip_received {false};
    smart_clock_control::SyncSchedule sync_schedule;
    bool sync_started {false};
    BUTTON button;
    QueueHandle_t button_events {nullptr};
    TaskHandle_t network_task_handle {nullptr};
    static void network_task_entry(void* arg);
    void network_task();
    void handle_button_event(const BUTTON::Event& event);
    void wifi_run();
    static constexpr uint32_t BUTTON_IDLE_MS = 5 * 60 * 1000;
    smart_clock_control::DownloadWindow download_window {pdMS_TO_TICKS(BUTTON_IDLE_MS)};
    std::mutex session_mutex;
    std::atomic<bool> time_sync_stop_requested {false};
    httpd_handle_t data_server {nullptr}; // Only managed by the SMART_CLOCK network task.
    bool download_enabled();
    esp_err_t start_data_server();
    static SensorData http_snapshot(void* arg);
    static bool http_begin(void* arg);
    static void http_end(void* arg);
    static bool http_sd_available(void* arg);
    SensorHttpAccess http_access {this, http_snapshot, http_begin, http_end, http_sd_available, &sd_card};
    CLOCK_DISPLAY::Context display_context();
    void cleanup(); // Startup rollback only; all application tasks are still gated.
    SensorData sensor_snapshot(){
      std::lock_guard<std::mutex> guard(sensor_mutex);
      return sensor_data;
    }

    i2c_base::I2C i2c;
    BME280 bme280;
    SCD40 scd40;
    SD_CARD sd_card;
    WIFI wifi;
    SNTP sntp;
    
    QueueHandle_t co2_buffer {NULL};
    QueueHandle_t bme280_results_buffer {NULL};

    TaskHandle_t  sensor_task_handle {NULL};
    std::atomic<TaskHandle_t> update_display_handle {nullptr};
    std::atomic<bool> application_ready {false};

    esp_err_t create_update_display_task(const char* pname, uint16_t stack_size, UBaseType_t task_priority);
    esp_err_t create_monitor_sensor_task(const char* pname, uint16_t stack_size, UBaseType_t task_priority);
    
    static void get_update_display_task_entry_point(void* arg);
    static void get_monitor_sensor_task_entry_point(void* arg);
    static void wifi_connected_event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data);
    static void sntp_time_sync_complete(void* arg);
    static void notify_display_update(void* arg);
 
    void update_display_task(); 
    void monitor_sensor_task();


  
  public:
    WIFI::state_e wifi_state {WIFI::state_e::NOT_INITIALIZED};
    std::atomic<int> last_sync_year {-1};
    std::atomic<int> last_sync_yday {-1};

    
    SMART_CLOCK();
    esp_err_t init(void);
    esp_err_t run(void);
}; 
