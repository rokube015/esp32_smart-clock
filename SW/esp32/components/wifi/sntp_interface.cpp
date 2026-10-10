#include <iostream>

#include "esp_log.h"
#include "sntp_interface.h"

//Statics
std::chrono::_V2::system_clock::time_point SNTP::mlast_update{};

std::atomic<bool> SNTP::mis_running{false};
std::mutex SNTP::msync_mutex;
std::mutex SNTP::mcontrol_mutex;
static void (*sntp_time_sync_callback)(void*) = nullptr;
static void* sntp_time_sync_context = nullptr;

void SNTP::callback_on_ntp_update(timeval *tv){
  std::cout << "NTP updated, current time is: " << time_now_ascii() << '\n';
  void (*callback)(void*) = nullptr;
  void* context = nullptr;
  {
    std::lock_guard<std::mutex> guard(msync_mutex);
    mlast_update = std::chrono::system_clock::now();
    callback = sntp_time_sync_callback;
    context = sntp_time_sync_context;
  }
  if(callback != nullptr){ callback(context); }
}

SNTP::SNTP(void){
  esp_log_level_set(SNTP_TAG, ESP_LOG_INFO);
  ESP_LOGI(SNTP_TAG, "set SNTP_TAG log level: %d", ESP_LOG_ERROR);
} 
SNTP::~SNTP(void){ 
  esp_sntp_stop();
}

esp_err_t SNTP::init(void){
  std::lock_guard<std::mutex> guard(mcontrol_mutex);
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
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
      esp_sntp_setservername(1, "pool.ntp.com");
#endif

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

bool SNTP::is_time_synchronized(void){
  std::lock_guard<std::mutex> guard(msync_mutex);
  return mlast_update.time_since_epoch().count() != 0;
}

void SNTP::set_time_sync_callback(void (*callback)(void*), void* context){
  std::lock_guard<std::mutex> guard(msync_mutex);
  sntp_time_sync_callback = callback;
  sntp_time_sync_context = context;
}

bool SNTP::set_update_interval(uint32_t ms, const bool immediate){
  std::lock_guard<std::mutex> guard(mcontrol_mutex);
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
  std::lock_guard<std::mutex> guard(msync_mutex);
  return time_point_now() - mlast_update;
}

[[nodiscard]] std::chrono::seconds SNTP::epoch_seconds(void){
  return std::chrono::duration_cast<std::chrono::seconds>(time_point_now().time_since_epoch());
}

[[nodiscard]] const char* SNTP::time_now_ascii(void){
  const std::time_t time_now{std::chrono::system_clock::to_time_t(time_point_now())};

  thread_local char buffer[32];
  std::tm local_time {};
  if(localtime_r(&time_now, &local_time) == nullptr){ return "invalid time"; }
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_time);
  return buffer;
}

esp_err_t SNTP::get_daytime(char* ptime_string, size_t time_string_size){
  esp_err_t r = ESP_OK;
  const std::time_t time_now{std::chrono::system_clock::to_time_t(time_point_now())};
  std::tm local_time {};
  if(localtime_r(&time_now, &local_time) == nullptr){ return ESP_FAIL; }
  const std::tm* plocal_time = &local_time;
  
  char date[40] {};
  char week[16] {};
  static constexpr const char* weekdays[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  // ISO week and weekday: week 01 contains January 4; Monday=1, Sunday=7.
  if(std::strftime(date, sizeof(date), "%Y/%m/%d", plocal_time) == 0 ||
     std::strftime(week, sizeof(week), "W%V.%u", plocal_time) == 0){ return ESP_FAIL; }
  const int length = snprintf(ptime_string, time_string_size, "%s %s / %s", date, weekdays[local_time.tm_wday], week);
  if(length < 0 || static_cast<size_t>(length) >= time_string_size){
    r = ESP_FAIL;
    ESP_LOGW(SNTP_TAG, "time_string buffer size is too small.");
  }

  return r;
}

esp_err_t SNTP::get_time(char* ptime_string, size_t time_string_size){
  esp_err_t r = ESP_OK;
  const std::time_t time_now{std::chrono::system_clock::to_time_t(time_point_now())};
  std::tm local_time {};
  if(localtime_r(&time_now, &local_time) == nullptr){ return ESP_FAIL; }
  const std::tm* plocal_time = &local_time;
  
  if(std::strftime(ptime_string, time_string_size, "%H:%M", plocal_time) == 0){
    r = ESP_FAIL;
    ESP_LOGW(SNTP_TAG, "time_string buffer size is too small.");
  }

  return r;
}

esp_err_t SNTP::get_logtime(char* time_string, size_t time_string_size){
  esp_err_t r = ESP_OK;
  const std::time_t time_now{std::chrono::system_clock::to_time_t(time_point_now())};
  std::tm local_time {};
  if(localtime_r(&time_now, &local_time) == nullptr){ return ESP_FAIL; }
  const std::tm* plocal_time = &local_time;
  
  if(std::strftime(time_string, time_string_size, "%Y/%m/%d, %a, %H:%M:%S", plocal_time) == 0){
    r = ESP_FAIL;
    ESP_LOGW(SNTP_TAG, "time_string buffer size is too small.");
  }

  return r;
}

