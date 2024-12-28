set(SOURCES ./sd_card.cpp)

idf_component_register(SRCS ${SOURCES}
  REQUIRES driver fatfs
  INCLUDE_DIRS .)

