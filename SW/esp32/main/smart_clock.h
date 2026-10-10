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

#include "LovyanGFX.hpp"

class SMART_CLOCK final{
  private:
    constexpr static const char* SMART_CLOCK_TAG = "smart_clock"; 
  
    constexpr static uint16_t UPDATE_DISPLAY_INTERVAL {60 * 1000}; //[ms]  
    DMA_ATTR static LGFX_Sprite black_sprite;
    struct SensorData{
      uint16_t co2 {0};
      float temperature {0};
      double humidity {0};
      float pressure {0};
      bool valid {false};
    };
    SensorData sensor_data;
    std::mutex sensor_mutex;
    bool sd_ready {false};
    bool display_ready {false};
    SensorData sensor_snapshot(){
      std::lock_guard<std::mutex> guard(sensor_mutex);
      return sensor_data;
    }
    const char file_path[50] = "/sensor_log.csv";
    constexpr static uint8_t WHITE  {255};
    constexpr static uint8_t BLACK  {0};

    i2c_base::I2C i2c;
    BME280 bme280;
    SCD40 scd40;
    EPAPER4IN26 e_paper;
    SD_CARD sd_card;
    WIFI wifi;
    SNTP sntp;
    
    QueueHandle_t co2_buffer {NULL};
    QueueHandle_t bme280_results_buffer {NULL};

    TimerHandle_t update_display_timer_handle {NULL}; 
    TaskHandle_t  sensor_task_handle {NULL};
    std::atomic<TaskHandle_t> update_display_handle {nullptr};
    std::atomic<bool> application_ready {false};

    esp_err_t create_update_display_timer_task(const char* pname);
    esp_err_t create_update_display_task(const char* pname, uint16_t stack_size, UBaseType_t task_priority);
    esp_err_t create_monitor_sensor_task(const char* pname, uint16_t stack_size, UBaseType_t task_priority);
    
    static void get_update_display_timer_task_entry_point(TimerHandle_t timer_handle);
    static void get_update_display_task_entry_point(void* arg);
    static void get_monitor_sensor_task_entry_point(void* arg);
    static void wifi_connected_event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data);
    static void sntp_time_sync_complete(void* arg);
    static void notify_display_update(void* arg);
 
    void update_display_timer_task();
    void update_display_task(); 
    void monitor_sensor_task();

    esp_err_t display_epaper(const SensorData& data);
  
  public:
    WIFI::state_e wifi_state {WIFI::state_e::NOT_INITIALIZED};
    std::atomic<int> last_sync_year {-1};
    std::atomic<int> last_sync_yday {-1};
    std::atomic<bool> daily_wifi_sync_requested {false};
    
    SMART_CLOCK();
    esp_err_t init(void);
    void wifi_run(void);
    esp_err_t run(void);
}; 
