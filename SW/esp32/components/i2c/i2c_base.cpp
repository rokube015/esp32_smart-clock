#include <cstring>

#include "freertos/FreeRTOS.h"
#include "i2c_base.h"
#include "esp_log.h"

namespace i2c_base{
  
  I2C::I2C(i2c_port_num_t port, i2c_mode_t mode){
    mport = port;
    mmode = mode;
  }

  I2C::~I2C(){
    i2c_del_master_bus(mi2c_bus_handle);
  }

  esp_err_t I2C::init(gpio_num_t sda_io_num, gpio_num_t scl_io_num, bool pullup_enable){
    esp_err_t r = ESP_OK;
    
    if(mmode == I2C_MODE_MASTER){
      mi2c_bus_config.i2c_port = mport;
      mi2c_bus_config.sda_io_num = sda_io_num;
      mi2c_bus_config.scl_io_num = scl_io_num;
      mi2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
      mi2c_bus_config.glitch_ignore_cnt = 7;
      mi2c_bus_config.flags.enable_internal_pullup = pullup_enable;
      
      r = i2c_new_master_bus(&mi2c_bus_config, &mi2c_bus_handle);
      if(r != ESP_OK){
        ESP_LOGE(I2C_BASE_TAG, "fail to set i2c port to master mode.");
      }
    }
    else{
      ESP_LOGE(I2C_BASE_TAG, "fail to set i2c port to slave mode");
    }
    
    return r;
  }
  
  i2c_master_bus_handle_t I2C::get_i2c_master_bus_handle(){
    return mi2c_bus_handle;
  }

  esp_err_t I2C::read_byte(i2c_master_dev_handle_t dev_handle, 
      uint8_t command, uint8_t* pread_data_buffer){
    esp_err_t r = ESP_OK;
    
    if(r == ESP_OK){ 
      r = i2c_master_transmit_receive(dev_handle, &command, 1, 
          pread_data_buffer, 1, pdMS_TO_TICKS(1000));
      
      if(r != ESP_OK){
        ESP_LOGE(I2C_BASE_TAG, "fail to read data from %x.", command);
      }
    }
    return r; 
  }

  esp_err_t I2C::write_byte(i2c_master_dev_handle_t dev_handle, 
      uint8_t command, const uint8_t write_data){
    esp_err_t r = ESP_OK;
    const uint8_t send_buffer[2] {command, write_data};
    
    if(r == ESP_OK){
      r = i2c_master_transmit(dev_handle, send_buffer, 2, pdMS_TO_TICKS(1000));
      if(r != ESP_OK){
        ESP_LOGE(I2C_BASE_TAG, "fail to send data to %x.", command);
      } 
    }
    return r;
  }
  
  esp_err_t I2C::read_data(i2c_master_dev_handle_t dev_handle, 
          uint8_t command, uint8_t* pread_data_buffer, size_t buffer_size){
    esp_err_t r = ESP_OK;
    
    if(r == ESP_OK){
      r = i2c_master_transmit_receive(dev_handle, &command, 1, pread_data_buffer, buffer_size, pdMS_TO_TICKS(1000));
      if(r != ESP_OK){
        ESP_LOGE(I2C_BASE_TAG, "fail to read data to %x.", command);
      }
    }
    return r;
  }

  esp_err_t I2C::write_data(i2c_master_dev_handle_t dev_handle, 
      uint8_t command, uint8_t* pwrite_data_buffer, size_t buffer_size){
    esp_err_t r = ESP_OK;

    uint8_t* send_buffer = (uint8_t*) malloc(buffer_size + 1);
    if(send_buffer == NULL){
      r = ESP_FAIL;
      ESP_LOGE(I2C_BASE_TAG, "fail to alloc buffer");
    }

    if(r == ESP_OK){
      send_buffer[0] = command;
      memcpy(send_buffer + 1, pwrite_data_buffer, buffer_size); 
      r = i2c_master_transmit(dev_handle, send_buffer, buffer_size, pdMS_TO_TICKS(1000));
      if(r != ESP_OK){
        ESP_LOGE(I2C_BASE_TAG, "fail to send data to %x.", command);
      } 
    }
    free(send_buffer);

    return r;
  }
}
