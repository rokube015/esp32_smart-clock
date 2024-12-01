#include "i2c_base.h"

namespace i2c_base{
  I2C::I2C(i2c_port_t port, size_t slv_rx_buf_len, size_t slv_tx_buf_len, int intr_alloc_flags){
    mport = port;
    mslv_rx_buf_len = slv_rx_buf_len;
    mslv_tx_buf_len = slv_tx_buf_len;
    mintr_alloc_flags = intr_alloc_flags;
  }

  I2C::~I2C(){
    i2c_driver_delete(mport);
  }

  esp_err_t I2C::init_master(int sda_io_num, int scl_io_num, uint32_t clk_speed, 
                             bool sda_pullup_en, bool scl_pullup_en, uint32_t clk_flags){
    esp_err_t r = ESP_OK;
    i2c_config_t i2c_config{};
    mmode = I2C_MODE_MASTER;

    i2c_config.mode = I2C_MODE_MASTER;
    i2c_config.sda_io_num = sda_io_num;
    i2c_config.scl_io_num = scl_io_num;
    i2c_config.master.clk_speed = clk_speed;
    i2c_config.sda_pullup_en = sda_pullup_en;
    i2c_config.scl_pullup_en = scl_pullup_en;
    i2c_config.clk_flags = clk_flags;
    
    r |= i2c_param_config(mport, &i2c_config);

    r |= i2c_driver_install(mport, mmode, mslv_rx_buf_len, mslv_tx_buf_len, 0);

    return r;
  }

  uint8_t I2C::read_register(uint8_t dev_addr, uint8_t reg_addr){
    uint8_t rx_buf{};

    i2c_master_write_read_device(mport, dev_addr, &reg_addr, 1, &rx_buf, 1, pdMS_TO_TICKS(1000));
    
   return rx_buf; 
  }

  esp_err_t I2C::write_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t tx_data){
    const uint8_t tx_buf[2] {reg_addr, tx_data};

    return i2c_master_write_to_device(mport, dev_addr, tx_buf, 2, pdMS_TO_TICKS(1000));
  }
  
  esp_err_t I2C::read_register_multiple_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t* rx_data, int length){
    esp_err_t r = ESP_OK;

    r = i2c_master_write_read_device(mport, dev_addr, &reg_addr, 1, rx_data, length, pdMS_TO_TICKS(1000));

    return r;
  }

  esp_err_t I2C::write_register_multiple_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t* tx_data, int length){
    esp_err_t r = ESP_OK;

    uint8_t buffer[I2C_LINK_RECOMMENDED_SIZE(3)] = {0};

    i2c_cmd_handle_t i2c_handle = i2c_cmd_link_create_static(buffer, sizeof(buffer));
    r |= i2c_master_start(i2c_handle);
    r |= i2c_master_write_byte(i2c_handle, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    r |= i2c_master_write_byte(i2c_handle, reg_addr, true);
    r |= i2c_master_write(i2c_handle, tx_data, length, true);
    r |= i2c_master_stop(i2c_handle);
    r |= i2c_master_cmd_begin(mport, i2c_handle, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete_static(i2c_handle);

    return r;
  }
}
