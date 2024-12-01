set(SOURCES main.cpp)
idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS .
                    REQUIRES bme280 i2c)

idf_component_add_link_dependency(FROM bme280 i2c) 
