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
  public:
    i2c_base::I2C i2c;
    BME280 bme280;
    SCD40 scd40;
    SD_CARD sd_card;
    WIFI wifi;
    SNTP sntp;

    void setup(void);
    void wifi_run(void);

    WIFI::state_e wifi_state {WIFI::state_e::NOT_INITIALIZED};
    }; 
