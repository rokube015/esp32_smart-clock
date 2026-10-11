#pragma once
#include "sensor_data.h"
#include "sd_card.h"
#include "esp_http_server.h"

// Callbacks acquire/release the coordinator's own mutexes; HTTP owns no shared lock.
struct SensorHttpAccess {
  void* context;
  SensorData (*snapshot)(void*);
  bool (*begin_request)(void*);
  void (*end_request)(void*);
  bool (*sd_available)(void*);
  SD_CARD* sd;
};
namespace sensor_http {
  esp_err_t register_handlers(httpd_handle_t server, SensorHttpAccess& access);
  esp_err_t data_index(httpd_req_t* request);
  esp_err_t latest_sensor_data(httpd_req_t* request);
  esp_err_t download_sensor_log(httpd_req_t* request);
}
