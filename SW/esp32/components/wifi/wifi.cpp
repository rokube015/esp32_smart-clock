#include <algorithm>

#include "esp_log.h"
#include "esp_mac.h"

#include "wifi.h"

//initialize statics
char WIFI::mmac_addr_cstr[] {};
std::mutex WIFI::mmutex {};
WIFI::state_e WIFI::mstate{state_e::NOT_INITIALIZED};
wifi_init_config_t WIFI::mwifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
wifi_config_t WIFI::mwifi_cfg {};

WIFI::WIFI(void){
  esp_log_level_set(WIFI_TAG, ESP_LOG_INFO);
  ESP_LOGI(WIFI_TAG, "set WIFI_TAG log level: %d", ESP_LOG_ERROR);
  
  esp_err_t r = ESP_OK;

  if (!mmac_addr_cstr[0]){
    r = mget_mac_addrs();
    if (r != ESP_OK){
      ESP_LOGE(WIFI_TAG, "fail to get chip mac addrs.");
      ESP_LOGE(WIFI_TAG, "restart esp32");
      esp_restart();
    }
  }
}

void WIFI::wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data){
  if(event_base == WIFI_EVENT){
    const wifi_event_t event_type{static_cast<wifi_event_t>(event_id)};

    switch(event_type){
      case WIFI_EVENT_STA_START:
        {
          std::lock_guard<std::mutex> state_guard(mmutex);
          mstate = state_e::READY_TO_CONNECT;
          break;
        }
      case WIFI_EVENT_STA_CONNECTED:
        {
          std::lock_guard<std::mutex> state_guard(mmutex);
          mstate = state_e::WAITING_FOR_IP;
          break;
        }
      case WIFI_EVENT_STA_DISCONNECTED:
        {
          std::lock_guard<std::mutex> state_guard(mmutex);
          mstate = state_e::DISCONNECTED;
          break;
        }
      default:
        break;
    }
  }
}

void WIFI::ip_event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data){
  if(event_base == IP_EVENT){
    const ip_event_t event_type{static_cast<ip_event_t>(event_id)};
    
    switch(event_type){
      case IP_EVENT_STA_GOT_IP:
        {
          std::lock_guard<std::mutex> stage_guard(mmutex);
          mstate = state_e::CONNECTED;
          break;
        }
      case IP_EVENT_STA_LOST_IP:
        {
          std::lock_guard<std::mutex>state_guard(mmutex);
          if(mstate != state_e::DISCONNECTED){
            mstate = state_e::WAITING_FOR_IP;
          }
          break;
        }

      default:
        break;
    }
  }
}

esp_err_t WIFI::begin(void){
  std::lock_guard<std::mutex>connect_guard(mmutex);

  esp_err_t r = ESP_OK;

  switch(mstate){
    case state_e::READY_TO_CONNECT:
    case state_e::DISCONNECTED:
      r = esp_wifi_connect();
      if(r == ESP_OK){
        mstate = state_e::CONNECTING;
      }
      break;
    case state_e::CONNECTING:
    case state_e::WAITING_FOR_IP:
    case state_e::CONNECTED:
      break;
    case state_e::NOT_INITIALIZED:
    case state_e::INITIALIZED:
    case state_e::ERROR:
      r = ESP_FAIL;
      break;
  }

  return r;
}

esp_err_t WIFI::mwifi_init(){
  std::lock_guard<std::mutex> mutex_guard(mmutex);

  esp_err_t r = ESP_OK;

  if(mstate == state_e::NOT_INITIALIZED){
    if(r == ESP_OK){
      r = esp_netif_init();
    }
    if(r == ESP_OK){
      const esp_netif_t *const p_netif = esp_netif_create_default_wifi_sta();
      if (p_netif == NULL){
        r = ESP_FAIL;
      }
    }
    if (r == ESP_OK){
      r = esp_wifi_init(&mwifi_init_cfg);
    }
    
    if(r == ESP_OK){
      r = esp_event_handler_instance_register(WIFI_EVENT, 
                                              ESP_EVENT_ANY_ID,
                                              &wifi_event_handler,
                                              nullptr,
                                              nullptr);
    }
    if(r == ESP_OK){
      r = esp_event_handler_instance_register(IP_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &ip_event_handler,
                                              nullptr,
                                              nullptr);
    }
    if(r == ESP_OK){
      r = esp_wifi_set_mode(WIFI_MODE_STA);
    }
    if(r == ESP_OK){
      mwifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
      mwifi_cfg.sta.pmf_cfg.capable = true;
      mwifi_cfg.sta.pmf_cfg.required = false;

      r = esp_wifi_set_config(WIFI_IF_STA, &mwifi_cfg);
    }
    if(r == ESP_OK){
      r = esp_wifi_start();
    }
    if(r == ESP_OK){
      mstate = state_e::INITIALIZED;
    }
  }
  else if(mstate == state_e::ERROR){
    mstate = state_e::NOT_INITIALIZED;
  }

  return r;
}

void WIFI::set_credentials(const char* ssid, const char* password){
  memcpy(mwifi_cfg.sta.ssid, ssid, std::min(strlen(ssid), sizeof(mwifi_cfg.sta.ssid)));
  memcpy(mwifi_cfg.sta.password, password, std::min(strlen(password), sizeof(mwifi_cfg.sta.password)));
}

esp_err_t WIFI::init(){
  esp_err_t r = ESP_OK;

  r = mwifi_init();

  return r;
}

esp_err_t WIFI::mget_mac_addrs(void){
  uint8_t mac_byte_buffer[6] {};

  esp_err_t r = ESP_OK;

  if(r == ESP_OK){
    r = esp_efuse_mac_get_default(mac_byte_buffer);
    if(r != ESP_OK){
      ESP_LOGE(WIFI_TAG, "fail to get esp32 default mac addrs.");
    }
  }
  
  if(r == ESP_OK){
    snprintf(mmac_addr_cstr, sizeof(mmac_addr_cstr), "%02X%02X%02X%02X%02X%02X",
        mac_byte_buffer[0],
        mac_byte_buffer[1],
        mac_byte_buffer[2],
        mac_byte_buffer[3],
        mac_byte_buffer[4],
        mac_byte_buffer[5]);
  }
  return r;
}
