idf_component_register(SRCS main.cpp smart_clock.cpp sensor_http.cpp
  INCLUDE_DIRS .
  REQUIRES bme280 scd40 i2c gpio button wifi sd_card clock_display sensor_recorder sensor_data esp_http_server esp_timer nvs_flash)
