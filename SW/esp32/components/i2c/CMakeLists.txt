set(SOURCES ./i2c_base.cpp)

idf_component_register(SRCS ${SOURCES}
  REQUIRES esp_driver_i2c esp_event
  INCLUDE_DIRS .)

