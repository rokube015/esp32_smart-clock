#include <cstring>

#include "freertos/FreeRTOS.h"
#include "i2c_base.h"
#include "esp_log.h"

namespace i2c_base{
  
  I2C::I2C(i2c_port_num_t port, i2c_mode_t mode){
    esp_log_level_set(I2C_BASE_TAG, ESP_LOG_INFO);
    ESP_LOGI(I2C_BASE_TAG, "set I2C_BASE_TAG log level: %d", ESP_LOG_INFO);

    mport = port;
    mmode = mode;
  }

  I2C::~I2C(){
    i2c_del_master_bus(mi2c_bus_handle);
  }

  esp_err_t I2C::take_i2c_port_semaphore(){
    esp_err_t r = ESP_OK;
    if(xSemaphoreTake(i2c_port_semaphore, pdMS_TO_TICKS(I2C_TIMEOUT)) == pdTRUE){
      r = ESP_OK;
    }
    else{
      ESP_LOGE(I2C_BASE_TAG, "fail to take i2c_port_semaphore.");
      r = ESP_FAIL;
    }
    return r;
  }

  esp_err_t I2C::release_i2c_port_semaphore(){
    esp_err_t r = ESP_OK;
    if(xSemaphoreGive(i2c_port_semaphore) == pdTRUE){
      r = ESP_OK;
    }
    else{
      ESP_LOGE(I2C_BASE_TAG, "fail to release i2c_port_semaphore.");
      r = ESP_FAIL;
    }
    return r; 
  }

  esp_err_t I2C::init(bool pullup_enable){
    esp_err_t r = ESP_OK;
    if(r == ESP_OK){ 
      i2c_port_semaphore = xSemaphoreCreateMutex();
      if(i2c_port_semaphore == NULL){
        ESP_LOGE(I2C_BASE_TAG, "fail to create i2c_port_semaphore.");
      }
    }
    
    if(r == ESP_OK){
      if(take_i2c_port_semaphore() == ESP_OK){
        if(mmode == I2C_MODE_MASTER){
          mi2c_bus_config.i2c_port = mport;
          mi2c_bus_config.sda_io_num = SDA_PIN;
          mi2c_bus_config.scl_io_num = SCL_PIN;
          mi2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
          mi2c_bus_config.glitch_ignore_cnt = 7;
          mi2c_bus_config.intr_priority = 0;
          mi2c_bus_config.trans_queue_depth = 0; 
          mi2c_bus_config.flags.enable_internal_pullup = pullup_enable;
          mi2c_bus_config.flags.allow_pd = 0;      
          r = i2c_new_master_bus(&mi2c_bus_config, &mi2c_bus_handle);
          if(r != ESP_OK){
            ESP_LOGE(I2C_BASE_TAG, "fail to set i2c port to master mode. error code:%s", esp_err_to_name(r));
          }
        }
        else{
          ESP_LOGE(I2C_BASE_TAG, "fail to set i2c port to slave mode");
        }
        r |= release_i2c_port_semaphore(); 
      }
    }
    return r;
  }
  
  i2c_master_bus_handle_t I2C::get_i2c_master_bus_handle(){
    return mi2c_bus_handle;
  }

  esp_err_t I2C::read_byte(i2c_master_dev_handle_t dev_handle, 
      const uint8_t* pcommand, size_t command_size,
      uint8_t* pread_data_buffer){
    esp_err_t r = ESP_OK;

    if(r == ESP_OK){
      if(take_i2c_port_semaphore() == ESP_OK){ 
        r = i2c_master_transmit_receive(dev_handle, pcommand, command_size, 
            pread_data_buffer, 1, pdMS_TO_TICKS(1000));
        if(r != ESP_OK){
          ESP_LOGE(I2C_BASE_TAG, "fail to read data.");
        }
        r |= release_i2c_port_semaphore(); 
      }
    }
    return r; 
  }

  esp_err_t I2C::write_byte(i2c_master_dev_handle_t dev_handle, 
      const uint8_t* pcommand, size_t command_size, 
      const uint8_t write_data){
    esp_err_t r = ESP_OK;
    
    uint8_t* send_buffer = (uint8_t*)malloc(command_size + 1);
    if(send_buffer == NULL){
      r = ESP_FAIL;
      ESP_LOGE(I2C_BASE_TAG, "fail to alloc buffer.");
    }

    if(r == ESP_OK){
      memcpy(send_buffer, pcommand, command_size); 
      memcpy(send_buffer + command_size, &write_data, 1); 
      if(take_i2c_port_semaphore() == ESP_OK){ 
        r = i2c_master_transmit(dev_handle, send_buffer, 2, pdMS_TO_TICKS(I2C_TIMEOUT));
        if(r != ESP_OK){
          ESP_LOGE(I2C_BASE_TAG, "fail to send data.");
        }
        r |= release_i2c_port_semaphore();
      }
    }
    return r;
  }
  
  esp_err_t I2C::read_data(i2c_master_dev_handle_t dev_handle, 
          const uint8_t* pcommand, size_t command_size, 
          uint8_t* pread_data_buffer, size_t read_buffer_size){
    esp_err_t r = ESP_OK;

    if(r == ESP_OK){
      if(take_i2c_port_semaphore() == ESP_OK){ 
        r = i2c_master_transmit_receive(dev_handle, pcommand, command_size, pread_data_buffer, read_buffer_size, pdMS_TO_TICKS(I2C_TIMEOUT));
        if(r != ESP_OK){
          ESP_LOGE(I2C_BASE_TAG, "fail to read data. I2C Error:%s", esp_err_to_name(r));
        }
        r |= release_i2c_port_semaphore();
      }
    }
    return r;
  }

  esp_err_t I2C::write_data(i2c_master_dev_handle_t dev_handle, 
      const uint8_t* pcommand, size_t command_size, 
      const uint8_t* pwrite_data_buffer, size_t buffer_size){
    esp_err_t r = ESP_OK;
    
    size_t send_buffer_size = command_size + buffer_size;
    uint8_t* send_buffer = (uint8_t*) malloc(send_buffer_size);
    if(send_buffer == NULL){
      r = ESP_FAIL;
      ESP_LOGE(I2C_BASE_TAG, "fail to alloc buffer");
    }

    if(r == ESP_OK){
      memcpy(send_buffer, pcommand, command_size); 
      if(buffer_size != 0){
        memcpy(send_buffer + command_size, pwrite_data_buffer, buffer_size);
      } 
      if(take_i2c_port_semaphore() == ESP_OK){ 
        r = i2c_master_transmit(dev_handle, send_buffer, send_buffer_size, pdMS_TO_TICKS(I2C_TIMEOUT));
        if(r != ESP_OK){
          ESP_LOGE(I2C_BASE_TAG, "fail to send data.");
        }
        r |= release_i2c_port_semaphore();
      } 
    }
    free(send_buffer);
    return r;
  }
}
