#include "bme280.h"
#include "i2c_base.h"

static const char* BME280_TAG = "bme280";

i2c_master_dev_handle_t bme280_handle;

esp_err_t init_bme280(){
  esp_err_t r = ESP_OK;
  esp_log_level_set(BME280_TAG, ESP_LOG_DEBUG);
  
  i2c_device_config_t i2c_dev_conf = {    
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x76,
    .scl_speed_hz = 100000,
  };

  r = i2c_master_bus_add_device(bus_handle, &i2c_dev_conf, &bme280_handle);
  ESP_ERROR_CHECK(r);

  return r;
}

esp_err_t check_bme280_chip_id(){
  esp_err_t r = ESP_OK;

  uint8_t chip_id_addrs = 0xD0;
  uint8_t chip_id = 0x00;

  if(r == ESP_OK){
    r = i2c_master_transmit_receive(bme280_handle, &chip_id_addrs, 1, &chip_id, 1, -1);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to bme280 chip id.");
    }
  }

  if(r == ESP_OK){
    ESP_LOGI(BME280_TAG, "bme280 chip id is %d.", chip_id);
    if(chip_id == 0x60){
      ESP_LOGI(BME280_TAG, "bme280 id check is success.");
    }
    else{
      ESP_LOGE(BME280_TAG, "bme280 id check is fail.");
      r = ESP_FAIL;
    }
  }

  return r;
}



