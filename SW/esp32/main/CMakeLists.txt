set(SOURCES main.cpp)
idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS .
                    REQUIRES bme280 scd40 i2c wifi sd_card)

idf_component_add_link_dependency(FROM bme280 scd40 i2c wifi sd_card) 
