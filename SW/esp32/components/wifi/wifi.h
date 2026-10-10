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
      PROVISIONING,
      ERROR
    };

  private:
    constexpr static const char* WIFI_TAG = "wifi"; 
    constexpr static uint8_t WIFI_CONNECT_FAILURE_LIMIT {5};
    
    static esp_err_t mwifi_init();
    static esp_err_t mstart_provisioning(bool initialize_manager);
    static wifi_init_config_t mwifi_init_cfg;
    static bool mprovisioning;
    static uint8_t mconnection_failures;
    static char mprovisioning_pop[17];
    static char mprovisioning_ssid[33];

    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data);

    static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data);

    static void provisioning_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data);

    static state_e mstate;
    static esp_err_t mget_mac_addrs(void);
    static char mmac_addr_cstr[13];
    static std::mutex mmutex;

  public:
    WIFI(void);

    esp_err_t init();
    esp_err_t begin(void);
    esp_err_t start(void);
    esp_err_t stop(void);

    static state_e get_state(void){
      std::lock_guard<std::mutex> guard(mmutex);
      return mstate;
    }
    constexpr static const char* get_mac_addrs(void){return mmac_addr_cstr;}
    static void get_setup_info(char (&ssid)[33], char (&pop)[17]){
      std::lock_guard<std::mutex> guard(mmutex);
      memcpy(ssid, mprovisioning_ssid, sizeof(ssid));
      memcpy(pop, mprovisioning_pop, sizeof(pop));
    }
};

