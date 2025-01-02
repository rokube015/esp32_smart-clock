set(SOURCES main.cpp smart_clock.cpp)
idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS .
                    REQUIRES bme280 scd40 display i2c gpio wifi sd_card)

idf_component_add_link_dependency(FROM bme280 scd40 display i2c wifi sd_card) 
