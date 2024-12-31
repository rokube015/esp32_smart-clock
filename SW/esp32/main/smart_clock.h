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
#include "sd_card.h"
#include "sntp_interface.h"

class SMART_CLOCK final{
  private:
    constexpr static const char* SMART_CLOCK_TAG = "smart_clock"; 
    const char file_path[50] = "/sensor_log.csv";
  public:
    WIFI::state_e wifi_state {WIFI::state_e::NOT_INITIALIZED};
    
    i2c_base::I2C i2c;
    BME280 bme280;
    SCD40 scd40;
    SD_CARD sd_card;
    WIFI wifi;
    SNTP sntp;

    float temperature {0.0};  //[degree Celsius]
    float pressure    {0.0};  //[hPa]
    double humidity   {0.0};  //[%]
    uint16_t co2      {0};    //[ppm]
    
    void init(void);
    void wifi_run(void);
    void run(void);

    }; 
