#include "esp_log.h"
#include "esp_check.h"

#include "aqm0802a.h"

AQM0802A::AQM0802A(){
  esp_log_level_set(AQM0802A_TAG, ESP_LOG_INFO);
  ESP_LOGI(AQM0802A_TAG, "set AQM0802A_TAG log level: %d", ESP_LOG_INFO);
}

esp_err_t AQM0802A::init_i2c(void){
  esp_err_t r = ESP_OK;
  ESP_LOGI(AQM0802A_TAG, "initialize i2c device.");
  if(r == ESP_OK){
   i2c_device_config_t i2c_device_config = { 
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = DEVICE_ADDRS,
    .scl_speed_hz = CLK_SPEED_HZ,
    .scl_wait_us = 0,
   };
   i2c_device_config.flags.disable_ack_check = true;
   r = i2c_master_bus_add_device(pmi2c->get_i2c_master_bus_handle(),
       &i2c_device_config, &mi2c_device_handle); 
  }
  if(r != ESP_OK){
    ESP_LOGE(AQM0802A_TAG, "fail to add AQM0802A to i2c port");
  }

  return r;
}

esp_err_t AQM0802A::send_init_command(){
  esp_err_t r = ESP_OK;
  ESP_LOGI(AQM0802A_TAG, "send initialize command.");
  if(r == ESP_OK){
    r = send_command(0x38);     // Function set
    r |= send_command(0x39);     // Function set
    r |= send_command(0x14);     // Internal OSC frequency
    r |= send_command(0x70);     // Contrast set
    r |= send_command(0x56);     // Power/ICON/Contrast set
    r |= send_command(0x6c);     // Follower control
    vTaskDelay(pdMS_TO_TICKS(300));                 
    r |= send_command(0x38);     // Function set
    r |= send_command(0x0c);     // Display ON/OFF control
    r |= send_command(0x01);     // Clear Display
    vTaskDelay(pdMS_TO_TICKS(10));
    if(r != ESP_OK){
     ESP_LOGE(AQM0802A_TAG, "fail to send init command.");
    } 
  }

  return r;
}

esp_err_t AQM0802A::send_command(const uint8_t command){
  esp_err_t r = ESP_OK;

  uint8_t control_byte = 0x00;

  if(r == ESP_OK){
    ESP_LOGI(AQM0802A_TAG, "send command:%x", command);
    r = pmi2c->write_byte(mi2c_device_handle, &control_byte, 1, command);
  }
  vTaskDelay(pdMS_TO_TICKS(1));
  return r;
}

esp_err_t AQM0802A::send_data(const uint8_t data){
  esp_err_t r = ESP_OK;

  uint8_t command = 0x40;
  
  if(r == ESP_OK){
    r = pmi2c->write_byte(mi2c_device_handle, &command, 1, data);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return r;
}

esp_err_t AQM0802A::init(i2c_base::I2C* pi2c){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){
    r = xresetb.init(GPIO_NUM_47, true);
  }
  if(r == ESP_OK){
    r = execute_hw_reset();
  }
  if(r == ESP_OK){
    pmi2c = pi2c;
    r = init_i2c();
    if(r != ESP_OK){
      ESP_LOGE(AQM0802A_TAG, "fail to initialize AQM0802A.");
    }
  }

  if(r == ESP_OK){
    r = send_init_command();
    if(r != ESP_OK){
      ESP_LOGE(AQM0802A_TAG, "fail to initialize AQM0802A.");
    }
  }
  return r;
}

esp_err_t AQM0802A::print_string(const char* pstring){
  esp_err_t r = ESP_OK;

  while(*pstring != '\0'){
    r = send_data(*pstring);
    ESP_LOGI(AQM0802A_TAG, "print:%c", *pstring);
    if(r != ESP_OK){
      ESP_LOGE(AQM0802A_TAG, "fail to send data.");
      break;
    }
    pstring++;
  }
  return r;
}

esp_err_t AQM0802A::clear_display(){
  esp_err_t r = ESP_OK;
  
  r = send_command(CLEAR_DISPLAY);
  if(r != ESP_OK){
    ESP_LOGE(AQM0802A_TAG, "fail to send CLEAR_DISPLAY command.");
  }
  return r;
}

esp_err_t AQM0802A::return_cursor_home(){
  esp_err_t r = ESP_OK;

  r = send_command(RETURN_HOME);
  if(r != ESP_OK){
    ESP_LOGE(AQM0802A_TAG, "fail to send RETURN_HOME command.");
  }
  return r;
}

esp_err_t AQM0802A::set_cursor_pos(uint8_t row, uint8_t col){
  esp_err_t r = ESP_OK;
  if(row < MIN_ROW_POS || row > MAX_ROW_POS){
    ESP_LOGW(AQM0802A_TAG, "the set row value is not supported.");
    r = ESP_ERR_INVALID_ARG;
  }
  if(col < MIN_COL_POS || col > MAX_COL_POS){
    ESP_LOGW(AQM0802A_TAG, "the set col value is not supported.");
    r = ESP_ERR_INVALID_ARG;
  } 
  if(r == ESP_OK){  
    uint8_t pos_addr = ROW_ADDR_OFFSET*row + col;
    r = send_command(SET_DDRAM_ADDRESS | pos_addr);
    if(r != ESP_OK){
      ESP_LOGE(AQM0802A_TAG, "fail to send DDRAM address.");
    }
  }

  return r;
}

esp_err_t AQM0802A::execute_hw_reset(){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){
    r = xresetb.on();
    vTaskDelay(pdMS_TO_TICKS(500));
    r = xresetb.off();
    vTaskDelay(pdMS_TO_TICKS(500));
    if(r != ESP_OK){
      ESP_LOGE(AQM0802A_TAG, "fail to external hw reset.");
    }
  }
  return r;
}

