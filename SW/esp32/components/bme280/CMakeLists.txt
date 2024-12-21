set(SOURCES ./bme280.cpp)

idf_component_register(SRCS ${SOURCES}
  REQUIRES driver esp_event i2c
  INCLUDE_DIRS .)

idf_component_add_link_dependency(FROM i2c)
