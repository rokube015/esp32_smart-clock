#pragma once

#include <cstring>
#include <mutex>

#include "esp_wifi.h"
#include "esp_event.h"

class WIFI{
  public:
    enum class state_e{
      NOT_INITIALIZED,
      INITIALIZED,
      READY_TO_CONNECT,
      CONNECTING,
      WAITING_FOR_IP,
      CONNECTED,
      DISCONNECTED,
      ERROR
    };

  private:
    constexpr static const char* WIFI_TAG = "wifi"; 
    
    static esp_err_t mwifi_init();
    static wifi_init_config_t mwifi_init_cfg;
    static wifi_config_t mwifi_cfg;

    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data);

    static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data);

    static state_e mstate;
    static esp_err_t mget_mac_addrs(void);
    static char mmac_addr_cstr[13];
    static std::mutex mmutex;

  public:
    WIFI(void);

    void set_credentials(const char* pssid, const char* ppassword);
    esp_err_t init();
    esp_err_t begin(void);

    constexpr static const state_e &get_state(void){return mstate;}
    constexpr static const char* get_mac_addrs(void){return mmac_addr_cstr;}
};

