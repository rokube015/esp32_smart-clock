#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "scd40.h"
#include "sd_card.h"

#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"
#include "wifi_pass.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi_pass.h"
#define ESP_MAXIMUM_RETRY  5

static const char* MAIN_TAG = "app_main";

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define INET6_ADDRSTRLEN 48
#define CONFIG_SNTP_TIME_SERVER "pool.ntp.org"

static int s_retry_num = 0;

static void obtain_time(void);

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data){
  if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
    esp_wifi_connect();
  }
  else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
    if(s_retry_num < ESP_MAXIMUM_RETRY){
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(MAIN_TAG, "retry to connect to the AP");
    }
    else{
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(MAIN_TAG, "connect to the AP faul");
  }
  else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(MAIN_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void time_sync_notification_cb(struct timeval *tv){
  ESP_LOGI(MAIN_TAG, "Notification of a time synchronization event");
}

static void print_servers(void){
  ESP_LOGI(MAIN_TAG, "List of configured NTP servers:");

  for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i){
    if (esp_sntp_getservername(i)){
      ESP_LOGI(MAIN_TAG, "server %d: %s", i, esp_sntp_getservername(i));
    } else {
      // we have either IPv4 or IPv6 address, let's print it
      char buff[INET6_ADDRSTRLEN];
      ip_addr_t const *ip = esp_sntp_getserver(i);
      if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
        ESP_LOGI(MAIN_TAG, "server %d: %s", i, buff);
    }
  }
}

static void obtain_time(void){
  s_wifi_event_group = xEventGroupCreate();
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(MAIN_TAG, "ESP_WIFI_MODE_STA");

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
      /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
       * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
       * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
       * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
       */
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

  ESP_LOGI(MAIN_TAG, "wifi_init_sta finished.");

  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
      WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
      pdFALSE,
      pdFALSE,
      portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(MAIN_TAG, "connected to ap SSID:%s password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(MAIN_TAG, "Failed to connect to SSID:%s, password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
  } else {
    ESP_LOGE(MAIN_TAG, "UNEXPECTED EVENT");
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
    ESP_LOGI(MAIN_TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
  }
  time(&now);
  localtime_r(&now, &timeinfo);
}

void app_main(void){
  esp_err_t r = ESP_OK;
  esp_log_level_set(MAIN_TAG, ESP_LOG_DEBUG);
 
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  // Is time set? If not, tm_year will be (1970 - 1900).
  if (timeinfo.tm_year < (2024 - 1900)) {
    ESP_LOGI(MAIN_TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
    obtain_time();
    // update 'now' variable with current time
    time(&now);
  }

  char strftime_buf[64];

  // Set timezone to Eastern Standard Time and print local time
  setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(MAIN_TAG, "The current date/time in New York is: %s", strftime_buf);

  // Set timezone to JST Standard Time
  setenv("TZ", "JST-9", 1);
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(MAIN_TAG, "The current date/time in Japan is: %s", strftime_buf);

  if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH) {
    struct timeval outdelta;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
      adjtime(NULL, &outdelta);
      ESP_LOGI(MAIN_TAG, "Waiting for adjusting time ... outdelta = %jd sec: %li ms: %li us",
          (intmax_t)outdelta.tv_sec,
          outdelta.tv_usec/1000,
          outdelta.tv_usec%1000);
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
  }

  const char* pScd40_data_filepath = MOUNT_POINT"/scd40.txt";
  char sdcard_write_data[MAX_SDCARD_LINE_CHAR_SIZE] = "\0";
  
  scd40_value_t scd40_value = {
    .co2 = 0,
    .temperature = 0.0, 
    .relative_humidity = 0.0
  };
  
  float temperature_offset = 0.0;
  
  if(r == ESP_OK){
    r = init_scd40();
  }
  if(r != ESP_OK){
    ESP_LOGE(MAIN_TAG, "faild to set up scd40.");
  }

  if(r == ESP_OK){
    r = check_scd40_serial_number();
  }
  
  if(r == ESP_OK){
    r = get_scd40_temperature_offset(&temperature_offset);
    if(r == ESP_OK){
      ESP_LOGI(MAIN_TAG, "default temperature_offset is %f", temperature_offset);
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
  }

  if(r == ESP_OK){
    temperature_offset = 0.0;
    ESP_LOGI(MAIN_TAG, "set temperature offset: %f", temperature_offset);
    r = set_scd40_temperature_offset(temperature_offset);
    vTaskDelay(5/ portTICK_PERIOD_MS);
  }
  if(r == ESP_OK){
    ESP_LOGI(MAIN_TAG, "initilize SD Card setup.");
    r =  init_sd_card();
  }
  
  if(r == ESP_OK){
    snprintf(sdcard_write_data, sizeof(sdcard_write_data), "CO2[rpm] \tTemperature[degree] \tHumidity[%%RH]\n");
    r = write_sd_card_file(pScd40_data_filepath, sdcard_write_data, 'w');
  }
  
  while(1){
    if(r == ESP_OK){
      r = start_scd40_periodic_measurement();
    }
    if(r == ESP_OK){
      vTaskDelay(5000/ portTICK_PERIOD_MS);
      r = get_scd40_sensor_data(&scd40_value);
      ESP_LOGI(MAIN_TAG, "co2:%d, temperature:%f, humidity:%f",
          scd40_value.co2, scd40_value.temperature, scd40_value.relative_humidity);
    }
    if(r == ESP_OK){
      vTaskDelay(5000/ portTICK_PERIOD_MS);
      r = stop_scd40_periodic_measurement();
    }
    if(r == ESP_OK){
      snprintf(sdcard_write_data, sizeof(sdcard_write_data),
          "%d\t%f\t%f\n",scd40_value.co2, scd40_value.temperature, scd40_value.relative_humidity);
      r = write_sd_card_file(pScd40_data_filepath, sdcard_write_data, 'a');
      ESP_LOGI(MAIN_TAG, "write scd40 data to sd card.");
      vTaskDelay(1000/ portTICK_PERIOD_MS);
    }
  }
}
