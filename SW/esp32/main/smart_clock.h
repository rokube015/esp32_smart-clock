#pragma once

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
    const char file_path[50] = "/sensor_log.csv";
    constexpr static uint8_t WHITE  {255};
    constexpr static uint8_t BLACK  {0};

    TimerHandle_t update_display_timer_handle {NULL}; 
    TaskHandle_t  sensor_task_handle {NULL};
    TaskHandle_t  update_display_handle {NULL};
    
    QueueHandle_t co2_buffer {NULL};
    QueueHandle_t bme280_results_buffer {NULL};
    
    esp_err_t display_epaper(char* pday_info, char* ptime_info); 
    
    void update_display_timer_task();
    void update_display_task(); 
    void monitor_sensor_task();

    esp_err_t create_update_display_timer_task(const char* pname);
    esp_err_t create_update_display_task(const char* pname, uint16_t stack_size, UBaseType_t task_priority);

    static void get_update_display_timer_task_entry_point(TimerHandle_t timer_handle);
    static void get_update_display_task_entry_point(void* arg);
    static void get_monitor_sensor_task_entry_point(void* arg);
  
  public:
    WIFI::state_e wifi_state {WIFI::state_e::NOT_INITIALIZED};

    i2c_base::I2C i2c;
    BME280 bme280;
    SCD40 scd40;
    EPAPER4IN26 e_paper;
    SD_CARD sd_card;
    WIFI wifi;
    SNTP sntp;
    
    DMA_ATTR static LGFX_Sprite black_sprite;
    
    BME280::results_data_t results_data {0.0, 0.0, 0.0};
    float temperature {0.0};  //[degree Celsius]
    float pressure    {0.0};  //[hPa]
    double humidity   {0.0};  //[%]
    uint16_t co2      {0};    //[ppm]

    SMART_CLOCK();
    void init(void);
    void wifi_run(void);
    esp_err_t run(void);
  
    esp_err_t create_monitor_sensor_task(const char* pname, uint16_t stack_size, UBaseType_t task_priority);
}; 
