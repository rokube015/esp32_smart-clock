set(SOURCES ./i2c_base.cpp)

idf_component_register(SRCS ${SOURCES}
  REQUIRES driver esp_event
  INCLUDE_DIRS .)

