#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "driver/i2c_master.h"
#include "esp_intr_alloc.h"

namespace i2c_base{
  class I2C{
    private:
      constexpr static const char* I2C_BASE_TAG = "i2c_base";
      constexpr static gpio_num_t SDA_PIN {GPIO_NUM_18};
      constexpr static gpio_num_t SCL_PIN {GPIO_NUM_17};
      constexpr static int I2C_TIMEOUT {1000}; // [ms]
      SemaphoreHandle_t i2c_port_semaphore {NULL};

      i2c_master_bus_handle_t mi2c_bus_handle; 
      i2c_master_bus_config_t mi2c_bus_config;
      uint16_t mslave_addrs{};
      i2c_port_num_t mport{};
      i2c_mode_t mmode{};
      esp_err_t take_i2c_port_semaphore();
      esp_err_t release_i2c_port_semaphore();

    public:
      I2C(i2c_port_num_t port = -1, 
          i2c_mode_t mode = I2C_MODE_MASTER);
      
      ~I2C();

      esp_err_t init(bool pullup_enable = true);
      i2c_master_bus_handle_t get_i2c_master_bus_handle();

      esp_err_t read_byte(i2c_master_dev_handle_t dev_handle, 
          const uint8_t* pcommand, size_t command_size, uint8_t* pread_data_buffer);
      
      esp_err_t write_byte(i2c_master_dev_handle_t dev_handle, 
          const uint8_t* pcommand, size_t command_size, const uint8_t write_data);
      
      esp_err_t read_data(i2c_master_dev_handle_t dev_handle, 
          const uint8_t* pcommand, size_t command_size, 
          uint8_t* pread_data_buffer, size_t read_buffer_size);
      
      esp_err_t write_data(i2c_master_dev_handle_t dev_handle, 
          const uint8_t* pcommand, size_t command_size, 
          const uint8_t* pwrite_data_buffer, size_t write_buffer_size);
  };
}
