#ifndef SCD40_H
#define SCD40_H
#include "driver/i2c_master.h"
#include "sdkconfig.h"
#include "esp_log.h"

#define PORT_NUMBER -1

esp_err_t init_scd40();

uint8_t calculate_scd40_crc(const uint8_t* data, uint16_t byte_size);

esp_err_t get_scd_40_serial_number(uint64_t* pserial_number);

#endif 
