set(SOURCES ./gpio_input.cpp
            ./gpio_output.cpp
            )

idf_component_register(SRCS ${SOURCES}
  REQUIRES esp_driver_gpio esp_event
  INCLUDE_DIRS .)
