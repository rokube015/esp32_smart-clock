#ifndef I2C_BASE_H
#define I2C_BASE_H
#include "driver/i2c_master.h"
#include "sdkconfig.h"
#include "esp_log.h"

extern i2c_master_bus_handle_t bus_handle;

esp_err_t i2c_port_init();

#endif

