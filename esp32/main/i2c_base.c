#include "i2c_base.h"

#define I2C_PORT_NUMBER -1

static const char* I2C_BASE_TAG = "i2c_base";
i2c_master_bus_handle_t bus_handle;

esp_err_t i2c_port_init(){
  esp_err_t r = ESP_OK;
  esp_log_level_set(I2C_BASE_TAG, ESP_LOG_DEBUG);

  i2c_master_bus_config_t i2c_bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_PORT_NUMBER,
    .scl_io_num = GPIO_NUM_22,
    .sda_io_num = GPIO_NUM_21,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true, 
  };

  r = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
  ESP_ERROR_CHECK(r);
  
  return r;
}
