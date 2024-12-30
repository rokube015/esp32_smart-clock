#pragma once

#include <ctime>
#include <chrono>
#include <iomanip>
#include <string>

#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi.h"

class SNTP : private WIFI{
  private: 
    constexpr static const char* SNTP_TAG = "SNTP"; 

    static std::chrono::_V2::system_clock::time_point mlast_update;
    static bool mis_running;
    static void callback_on_ntp_update(timeval *tv);

  public:
    SNTP(void);
    ~SNTP(void);

    static esp_err_t init(void);

    static bool set_update_interval(uint32_t ms, bool immediate = false);

    [[nodiscard]] bool is_running(void){return mis_running;}

    [[nodiscard]] static const auto time_point_now(void);

    [[nodiscard]] static const auto time_since_last_update(void);

    [[nodiscard]] static const char* time_now_ascii(void);

    [[nodiscard]] static std::chrono::seconds epoch_seconds(void);

    static esp_err_t get_logtime(char* ptimestamp, size_t timestamp_size);
    
};

