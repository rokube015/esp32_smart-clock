set(SOURCES ./aqm0802a.cpp ./e_paper.cpp)

idf_component_register(SRCS ${SOURCES}
  REQUIRES driver esp_event i2c gpio LovyanGFX
  INCLUDE_DIRS .)

idf_component_add_link_dependency(FROM i2c LovyanGFX)
