#include "sensor_http.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "freertos/task.h"

namespace sensor_http {
class HttpActivity {
  SensorHttpAccess& access;
public:
  bool admitted;
  explicit HttpActivity(SensorHttpAccess& value) : access(value), admitted(value.begin_request(value.context)) {}
  ~HttpActivity(){ if(admitted){ access.end_request(access.context); } }
};
esp_err_t data_index(httpd_req_t* request){
  HttpActivity activity(*static_cast<SensorHttpAccess*>(request->user_ctx));
  if(!activity.admitted){
    httpd_resp_set_status(request, "503 Service Unavailable");
    return httpd_resp_sendstr(request, "Press the clock button to enable downloads");
  }
  httpd_resp_set_type(request, "text/html; charset=utf-8");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_sendstr(request,
      "<!doctype html><html lang='ja'><meta charset='utf-8'><title>Smart Clock</title>"
      "<h1>Smart Clock</h1><p><a href='/sensor_log.csv'>センサ記録CSVをダウンロード</a></p>"
      "<p>取得開始時点までの記録です。操作がない状態が5分続くとWi-Fiを停止します。</p></html>");
}

esp_err_t latest_sensor_data(httpd_req_t* request){
  auto* instance = static_cast<SensorHttpAccess*>(request->user_ctx);
  HttpActivity activity(*instance);
  httpd_resp_set_type(request, "application/json");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  SensorData data = instance->snapshot(instance->context);
  char query[64] {}, since[16] {}, current[16] {};
  if(activity.admitted && data.all_valid() && httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK &&
      httpd_query_key_value(query, "since", since, sizeof(since)) == ESP_OK){
    // Keep the shared HTTP server responsive to downloads and shutdown.
    for(int attempt = 0; attempt < 20; ++attempt){
      snprintf(current, sizeof(current), "%lu", static_cast<unsigned long>(data.sequence));
      if(strcmp(since, current) != 0){ break; }
      vTaskDelay(pdMS_TO_TICKS(50));
      data = instance->snapshot(instance->context);
    }
    snprintf(current, sizeof(current), "%lu", static_cast<unsigned long>(data.sequence));
    if(data.all_valid() && strcmp(since, current) == 0){
      httpd_resp_set_status(request, "204 No Content");
      return httpd_resp_send(request, nullptr, 0);
    }
  }
  if(!activity.admitted || !data.all_valid()){
    httpd_resp_set_status(request, "503 Service Unavailable");
    return httpd_resp_sendstr(request, "{\"valid\":false}");
  }
  char body[256];
  snprintf(body, sizeof(body),
      "{\"valid\":true,\"co2_ppm\":%u,\"temperature_c\":%.2f,\"humidity_percent\":%.2f,\"pressure_hpa\":%.2f,\"sequence\":%lu}",
      data.co2, data.temperature, data.humidity, data.pressure, static_cast<unsigned long>(data.sequence));
  return httpd_resp_sendstr(request, body);
}

esp_err_t download_sensor_log(httpd_req_t* request){
  auto* instance = static_cast<SensorHttpAccess*>(request->user_ctx);
  HttpActivity activity(*instance);
  if(!activity.admitted || !instance->sd_available(instance->context)){
    httpd_resp_set_status(request, "503 Service Unavailable");
    return httpd_resp_sendstr(request, activity.admitted ?
        "SD card is unavailable" : "Download session expired");
  }
  FILE* file = nullptr;
  long remaining = 0;
  esp_err_t result = instance->sd->open_sensor_log(&file, &remaining);
  if(result != ESP_OK){
    return httpd_resp_send_err(request, result == ESP_ERR_NOT_FOUND ?
        HTTPD_404_NOT_FOUND : HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot open sensor log");
  }
  httpd_resp_set_type(request, "text/csv; charset=utf-8");
  httpd_resp_set_hdr(request, "Content-Disposition", "attachment; filename=\"sensor_log.csv\"");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  char buffer[1024];
  bool sent = false;
  while(remaining > 0 && result == ESP_OK){
    const size_t count = static_cast<size_t>(std::min(remaining, static_cast<long>(sizeof(buffer))));
    result = instance->sd->read_sensor_log(file, buffer, count);
    if(result == ESP_OK){
      result = httpd_resp_send_chunk(request, buffer, count);
      sent = true;
    }
    remaining -= count;
  }
  instance->sd->close_sensor_log(file);
  if(result != ESP_OK){
    ESP_LOGW("sensor_http", "Sensor log transfer failed: %s", esp_err_to_name(result));
    if(!sent){ return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot read sensor log"); }
    return ESP_FAIL; // Close connection without completing a truncated CSV.
  }
  return httpd_resp_send_chunk(request, nullptr, 0);
}

esp_err_t register_handlers(httpd_handle_t server, SensorHttpAccess& access){
  const struct { const char* uri; esp_err_t (*handler)(httpd_req_t*); } routes[] = {
    {"/", data_index}, {"/sensor_log.csv", download_sensor_log}, {"/api/sensors/latest", latest_sensor_data}
  };
  for(const auto& route : routes){
    httpd_uri_t uri {};
    uri.uri = route.uri; uri.method = HTTP_GET; uri.handler = route.handler; uri.user_ctx = &access;
    const esp_err_t result = httpd_register_uri_handler(server, &uri);
    if(result != ESP_OK){ return result; }
  }
  return ESP_OK;
}
}
