#include <algorithm>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"

#include "wifi.h"
#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_softap.h"

//initialize statics
char WIFI::mmac_addr_cstr[] {};
std::mutex WIFI::mmutex {};
WIFI::state_e WIFI::mstate{state_e::NOT_INITIALIZED};
wifi_init_config_t WIFI::mwifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
bool WIFI::mprovisioning{false};
uint8_t WIFI::mconnection_failures{0};
char WIFI::mprovisioning_pop[17] {};
char WIFI::mprovisioning_ssid[33] {};

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
          if(!mprovisioning){
            mstate = state_e::READY_TO_CONNECT;
          }
          break;
        }
      case WIFI_EVENT_STA_STOP:
        {
          std::lock_guard<std::mutex> state_guard(mmutex);
          if(!mprovisioning){
            mstate = state_e::INITIALIZED;
          }
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
          if(mprovisioning){
            mstate = state_e::PROVISIONING;
          }
          else{
            if(mconnection_failures < WIFI_CONNECT_FAILURE_LIMIT){
              ++mconnection_failures;
            }
            mstate = mconnection_failures >= WIFI_CONNECT_FAILURE_LIMIT
                ? state_e::ERROR : state_e::DISCONNECTED;
            if(mstate == state_e::ERROR){
              ESP_LOGW(WIFI_TAG, "Wi-Fi connection failed %u times; provisioning fallback required",
                       mconnection_failures);
            }
          }
          break;
        }
      case WIFI_EVENT_AP_START:
        ESP_LOGI(WIFI_TAG, "SoftAP started");
        break;
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
          mconnection_failures = 0;
          mstate = state_e::CONNECTED;
          break;
        }
      case IP_EVENT_STA_LOST_IP:
        {
          std::lock_guard<std::mutex>state_guard(mmutex);
          if(mstate == state_e::CONNECTED || mstate == state_e::WAITING_FOR_IP){
            mstate = state_e::WAITING_FOR_IP;
          }
          break;
        }
      case IP_EVENT_ASSIGNED_IP_TO_CLIENT:
        ESP_LOGI(WIFI_TAG, "SoftAP assigned an IP address to a client");
        break;

      default:
        break;
    }
  }
}

void WIFI::provisioning_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data){
  if(event_base != NETWORK_PROV_EVENT){
    return;
  }

  switch(event_id){
    case NETWORK_PROV_START:
      ESP_LOGI(WIFI_TAG, "Wi-Fi provisioning started");
      break;
    case NETWORK_PROV_WIFI_CRED_RECV:
      ESP_LOGI(WIFI_TAG, "Received Wi-Fi credentials for SSID: %s",
          static_cast<wifi_sta_config_t*>(event_data)->ssid);
      break;
    case NETWORK_PROV_WIFI_CRED_FAIL:
      ESP_LOGE(WIFI_TAG, "Wi-Fi provisioning credentials failed; restart to try again");
      break;
    case NETWORK_PROV_WIFI_CRED_SUCCESS:
      {
        std::lock_guard<std::mutex> state_guard(mmutex);
        mprovisioning = false;
        mstate = state_e::CONNECTED;
      }
      ESP_LOGI(WIFI_TAG, "Wi-Fi provisioning successful");
      break;
    case NETWORK_PROV_END:
      network_prov_mgr_deinit();
      ESP_LOGI(WIFI_TAG, "Wi-Fi provisioning stopped");
      break;
    default:
      break;
  }
}

esp_err_t WIFI::begin(void){
  std::unique_lock<std::mutex>connect_guard(mmutex);

  esp_err_t r = ESP_OK;

  if(mprovisioning){
    return ESP_OK;
  }

  if(mstate == state_e::ERROR){
    if(mconnection_failures < WIFI_CONNECT_FAILURE_LIMIT){
      return ESP_FAIL;
    }
    mprovisioning = true;
    mstate = state_e::PROVISIONING;
    connect_guard.unlock();
    r = esp_wifi_stop();
    if(r == ESP_OK){
      r = mstart_provisioning(true);
    }
    connect_guard.lock();
    if(r != ESP_OK){
      mprovisioning = false;
      mstate = state_e::ERROR;
      ESP_LOGE(WIFI_TAG, "Failed to start fallback Wi-Fi provisioning: %s",
               esp_err_to_name(r));
    }
    return r;
  }

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
    case state_e::PROVISIONING:
      break;
    case state_e::NOT_INITIALIZED:
    case state_e::INITIALIZED:
      r = ESP_FAIL;
      break;
    case state_e::ERROR:
      r = ESP_FAIL;
      break;
  }

  return r;
}

esp_err_t WIFI::start(void){
  {
    std::lock_guard<std::mutex> state_guard(mmutex);
    if(mprovisioning || mstate != state_e::INITIALIZED){
      return ESP_ERR_INVALID_STATE;
    }
  }

  return esp_wifi_start();
}

esp_err_t WIFI::stop(void){
  {
    std::lock_guard<std::mutex> state_guard(mmutex);
    if(mprovisioning){
      return ESP_ERR_INVALID_STATE;
    }
    mstate = state_e::INITIALIZED;
  }

  return esp_wifi_stop();
}

esp_err_t WIFI::mstart_provisioning(bool initialize_manager){
  const network_prov_mgr_config_t config = {
    .scheme = network_prov_scheme_softap,
    .scheme_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE,
    .app_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE,
    .network_prov_wifi_conn_cfg = {.wifi_conn_attempts = 0},
  };
  bool manager_initialized = !initialize_manager;
  esp_err_t r = ESP_OK;

  if(initialize_manager){
    r = network_prov_mgr_init(config);
    manager_initialized = (r == ESP_OK);
  }
  if(r == ESP_OK){
    char service_name[33] {};
    uint8_t pop_random[8] {};
    esp_fill_random(pop_random, sizeof(pop_random));
    char pop[17] {};
    snprintf(pop, sizeof(pop),
        "%02x%02x%02x%02x%02x%02x%02x%02x",
        pop_random[0], pop_random[1], pop_random[2], pop_random[3],
        pop_random[4], pop_random[5], pop_random[6], pop_random[7]);
    snprintf(service_name, sizeof(service_name), "SMART_CLOCK_%s", mmac_addr_cstr);
    {
      std::lock_guard<std::mutex> guard(mmutex);
      snprintf(mprovisioning_ssid, sizeof(mprovisioning_ssid), "%s", service_name);
      memcpy(mprovisioning_pop, pop, sizeof(pop));
    }
    r = network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_1,
        pop, service_name, nullptr);
    if(r == ESP_OK){
      ESP_LOGI(WIFI_TAG, "Connect to setup AP %s using Espressif's SoftAP Provisioning app", service_name);
      ESP_LOGI(WIFI_TAG, "Provisioning proof of possession: %s", pop);
    }
  }

  if(r != ESP_OK && manager_initialized){
    network_prov_mgr_deinit();
  }
  return r;
}

esp_err_t WIFI::mwifi_init(){
  std::unique_lock<std::mutex> mutex_guard(mmutex);

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
    if(r == ESP_OK){
      const esp_netif_t *const p_ap_netif = esp_netif_create_default_wifi_ap();
      if (p_ap_netif == NULL){
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
      const network_prov_mgr_config_t config = {
        .scheme = network_prov_scheme_softap,
        .scheme_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE,
        .app_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE,
        .network_prov_wifi_conn_cfg = {.wifi_conn_attempts = 0},
      };
      r = network_prov_mgr_init(config);
    }
    if(r == ESP_OK){
      r = esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID,
                                     &provisioning_event_handler, nullptr);
    }
    if(r == ESP_OK){
      bool provisioned = false;
      r = network_prov_mgr_is_wifi_provisioned(&provisioned);
      if(r == ESP_OK && provisioned){
        network_prov_mgr_deinit();
        r = esp_wifi_set_mode(WIFI_MODE_STA);
        if(r == ESP_OK){
          r = esp_wifi_start();
        }
        if(r == ESP_OK){
          mstate = state_e::INITIALIZED;
        }
      }
      else if(r == ESP_OK){
        mprovisioning = true;
        mstate = state_e::PROVISIONING;
        mutex_guard.unlock();
        r = mstart_provisioning(false);
        mutex_guard.lock();
        if(r != ESP_OK){
          mprovisioning = false;
          mstate = state_e::ERROR;
        }
      }
    }
  }
  else if(mstate == state_e::ERROR){
    mstate = state_e::NOT_INITIALIZED;
  }

  return r;
}

esp_err_t WIFI::init(){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){
    r = mwifi_init();
    if(r != ESP_OK){
      ESP_LOGE(WIFI_TAG, "fail to initialize wifi components.");
    }
  }

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
