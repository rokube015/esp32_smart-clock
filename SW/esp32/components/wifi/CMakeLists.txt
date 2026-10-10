set(SOURCES ./wifi.cpp ./sntp_interface.cpp)

idf_component_register(SRCS ${SOURCES}
  REQUIRES esp_wifi nvs_flash esp_event esp_netif lwip network_provisioning esp_hw_support
  INCLUDE_DIRS .)


