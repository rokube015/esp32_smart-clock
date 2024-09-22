#include "wifi.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_attr.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi_pass.h"

#include "wifi_pass.h"
#define ESP_MAXIMUM_RETRY  5

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define INET6_ADDRSTRLEN 48
#define CONFIG_SNTP_TIME_SERVER "pool.ntp.org"

static const char* WIFI_TAG = "wifi";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data);

void time_sync_notification_cb(struct timeval *tv);

static void print_servers();

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data){
  if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
    esp_wifi_connect();
  }
  else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
    if(s_retry_num < ESP_MAXIMUM_RETRY){
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
    }
    else{
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(WIFI_TAG, "connect to the AP faul");
  }
  else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void time_sync_notification_cb(struct timeval *tv){
  ESP_LOGI(WIFI_TAG, "Notification of a time synchronization event");
}

static void print_servers(void){
  ESP_LOGI(WIFI_TAG, "List of configured NTP servers:");

  for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i){
    if (esp_sntp_getservername(i)){
      ESP_LOGI(WIFI_TAG, "server %d: %s", i, esp_sntp_getservername(i));
    } else {
      // we have either IPv4 or IPv6 address, let's print it
      char buff[INET6_ADDRSTRLEN];
      ip_addr_t const *ip = esp_sntp_getserver(i);
      if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
        ESP_LOGI(WIFI_TAG, "server %d: %s", i, buff);
    }
  }
}

void obtain_time(){
  s_wifi_event_group = xEventGroupCreate();
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(WIFI_TAG, "ESP_WIFI_MODE_STA");

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default() );
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        NULL,
        &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &event_handler,
        NULL,
        &instance_got_ip));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = ESP_WIFI_SSID,
      .password = ESP_WIFI_PASS,
      .threshold.authmode = WIFI_AUTH_WPA2_PSK,
      .pmf_cfg = {
        .capable = true,
        .required = false
      },
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
      WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
      pdFALSE,
      pdFALSE,
      portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
  } else {
    ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
  }

  /*
   * This is the basic default config with one server and starting the service
   */
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
  config.sync_cb = time_sync_notification_cb;     // Note: This is only needed if we want

  esp_netif_sntp_init(&config);
  print_servers();

  // wait for time to be set
  time_t now = 0;
  struct tm timeinfo = { 0 };
  int retry = 0;
  const int retry_count = 15;
  while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
    ESP_LOGI(WIFI_TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
  }
  time(&now);
  localtime_r(&now, &timeinfo);
}
