#include <iostream>

#include "esp_log.h"
#include "sntp_interface.h"

//Statics
std::chrono::_V2::system_clock::time_point SNTP::mlast_update{};

bool SNTP::mis_running{false};

void SNTP::callback_on_ntp_update(timeval *tv){
  std::cout << "NTP updated, current time is: " << time_now_ascii() << '\n';
  mlast_update = std::chrono::system_clock::now();
}

SNTP::SNTP(void){
  esp_log_level_set(SNTP_TAG, ESP_LOG_INFO);
  ESP_LOGI(SNTP_TAG, "set SNTP_TAG log level: %d", ESP_LOG_ERROR);
} 
SNTP::~SNTP(void){ 
  esp_sntp_stop();
}

esp_err_t SNTP::init(void){
  esp_err_t r = ESP_OK;

  if (!mis_running){
    if(WIFI::get_state() != WIFI::state_e::CONNECTED){
      std::cout << "Failed to initialise SNTP, Wifi not connected\n";
      r = ESP_FAIL;
      return r; 
    }
    
    if(r == ESP_OK){
      setenv("TZ", "JST-9", 1); // Asia/Tokyo Time JST-9
      tzset();

      esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
      esp_sntp_setservername(0, "time.google.com");
      esp_sntp_setservername(1, "pool.ntp.com");

      sntp_set_time_sync_notification_cb(&callback_on_ntp_update);
      sntp_set_sync_interval(60 * 60 * 1000); // Update time every hour

      esp_sntp_init();

      ESP_LOGI(SNTP_TAG, "SNTP Initialised");

      mis_running = true;
    }
  }

  if (mis_running){
    r =  ESP_OK;
    return r;
  }
  r = ESP_FAIL;
  return r;
}

bool SNTP::set_update_interval(uint32_t ms, const bool immediate){
  if(mis_running){
    sntp_set_sync_interval(ms);
    if (immediate){
      sntp_restart();
    }
    return true;
  }
  return false;
}

[[nodiscard]] const auto SNTP::time_point_now(void){
  return std::chrono::system_clock::now();
}

[[nodiscard]] const auto SNTP::time_since_last_update(void){
  return time_point_now() - mlast_update;
}

[[nodiscard]] std::chrono::seconds SNTP::epoch_seconds(void){
  return std::chrono::duration_cast<std::chrono::seconds>(time_point_now().time_since_epoch());
}

[[nodiscard]] const char* SNTP::time_now_ascii(void){
  const std::time_t time_now{std::chrono::system_clock::to_time_t(time_point_now())};

  return std::asctime(std::localtime(&time_now));
}

esp_err_t SNTP::get_daytime(char* ptime_string, size_t time_string_size){
  esp_err_t r = ESP_OK;
  const std::time_t time_now{std::chrono::system_clock::to_time_t(time_point_now())};
  std::tm* plocal_time = std::localtime(&time_now);
  
  if(std::strftime(ptime_string, time_string_size, "%A %b %d", plocal_time) == 0){
    r = ESP_FAIL;
    ESP_LOGW(SNTP_TAG, "time_string buffer size is too small.");
  }

  return r;
}

esp_err_t SNTP::get_time(char* ptime_string, size_t time_string_size){
  esp_err_t r = ESP_OK;
  const std::time_t time_now{std::chrono::system_clock::to_time_t(time_point_now())};
  std::tm* plocal_time = std::localtime(&time_now);
  
  if(std::strftime(ptime_string, time_string_size, "%H:%M", plocal_time) == 0){
    r = ESP_FAIL;
    ESP_LOGW(SNTP_TAG, "time_string buffer size is too small.");
  }

  return r;
}

esp_err_t SNTP::get_logtime(char* time_string, size_t time_string_size){
  esp_err_t r = ESP_OK;
  const std::time_t time_now{std::chrono::system_clock::to_time_t(time_point_now())};
  std::tm* plocal_time = std::localtime(&time_now);
  
  if(std::strftime(time_string, time_string_size, "%Y/%m/%d, %a, %H:%M:%S", plocal_time) == 0){
    r = ESP_FAIL;
    ESP_LOGW(SNTP_TAG, "time_string buffer size is too small.");
  }

  return r;
}

