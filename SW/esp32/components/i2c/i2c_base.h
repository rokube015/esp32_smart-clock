#pragma once

#include "driver/i2c.h"
#include "esp_intr_alloc.h"

namespace i2c_base{
  class I2C{
    private:
      uint16_t mslave_addrs{};
      i2c_port_t mport{};
      i2c_mode_t mmode{};
      size_t mslv_rx_buf_len{};
      size_t mslv_tx_buf_len{};
      int mintr_alloc_flags{};

    public:
      I2C(i2c_port_t port, size_t slv_rx_buf_len = 0, size_t slv_tx_buf_len = 0, int intr_alloc_flags = 0);
      ~I2C();

      esp_err_t init_master(int sda_io_num,
                            int scl_io_num,
                            uint32_t clk_speed,
                            bool sda_pullup_en = false,
                            bool scl_pullup_en = false,
                            uint32_t clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL);

      uint8_t read_register(uint8_t dev_addr, uint8_t reg_addr);
      esp_err_t write_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t tx_data);
      esp_err_t read_register_multiple_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t* rx_data, int length);
      esp_err_t write_register_multiple_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t* tx_data, int length);
  };
}
