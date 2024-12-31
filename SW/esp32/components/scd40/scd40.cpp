#include <math.h>
#include "esp_log.h"
#include "esp_check.h"

#include "i2c_base.h"
#include "scd40.h"

SCD40::SCD40(){
  esp_log_level_set(SCD40_TAG, ESP_LOG_INFO);
  ESP_LOGI(SCD40_TAG, "set SCD40_TAG log level: %d", ESP_LOG_INFO);
}

esp_err_t SCD40::init_i2c(void){
  esp_err_t r = ESP_OK;
  
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
    ESP_LOGE(SCD40_TAG, "fail to add SCD40 to i2c port");
  }

  return r;
}

uint8_t SCD40::calculate_crc(const uint8_t* data, uint16_t byte_size){
  //check sum calculation method is written usermanual p.23 
  uint16_t current_byte = 0;
  uint8_t crc = 0xFF;
  const uint8_t CRC8_POLYNOMIAL = 0x31;
  uint8_t crc_bit = 0;
  /* calculates 8-Bit checksum with given polynomial */
  for(current_byte = 0; current_byte < byte_size; ++current_byte){
    crc ^= (data[current_byte]);
    for(crc_bit = 8; crc_bit > 0; --crc_bit){
      if(crc & 0x80){
        crc = (crc << 1) ^ CRC8_POLYNOMIAL;
      }
      else{
        crc = (crc << 1);
      }
    }
  }
  ESP_LOGI(SCD40_TAG, "Calculate CRC = %x", crc);
  return crc;
}

esp_err_t SCD40::init(i2c_base::I2C* pi2c){
  esp_err_t r = ESP_OK;
  
  if(r == ESP_OK){
    pmi2c = pi2c;
    r = init_i2c();
    if(r != ESP_OK){
      ESP_LOGE(SCD40_TAG, "fail to initialize SCD40.");
    }
  }

  if(r == ESP_OK){
    r = check_serial_number();
    if(r != ESP_OK){
      ESP_LOGE(SCD40_TAG, "fail to get SCD40 serial number.");
      r = stop_periodic_measurement();
      ESP_LOGI(SCD40_TAG, "send stop_periodic_measurement command.");
      vTaskDelay(pdMS_TO_TICKS(50));
      if(r == ESP_OK){
        r = check_serial_number();
      }
    }
  }
  return r;
}

esp_err_t SCD40::get_serial_number(uint64_t* pserial_number){
  esp_err_t r = ESP_OK;
  union{
    uint8_t arr[9];
    struct{
      uint8_t word_0[2];
      uint8_t crc_0;
      uint8_t word_1[2];
      uint8_t crc_1;
      uint8_t word_2[2];
      uint8_t crc_2;
    }data;
  }serial_number;
  
  if(r == ESP_OK){
    r = read_data(GET_SERIAL_NUMBER_COMMAND, serial_number.arr, sizeof(serial_number.arr));
    if(r != ESP_OK){
      ESP_LOGE(SCD40_TAG, "fail to read SCD40 serial number");
    }
  }

  if(r == ESP_OK){
    *pserial_number = ((uint64_t)serial_number.data.word_0[0] << 40 | (uint64_t)serial_number.data.word_0[1] << 32) |
      ((uint64_t)serial_number.data.word_1[0] << 24 | (uint64_t)serial_number.data.word_0[1] << 16) |
      ((uint64_t)serial_number.data.word_2[0] << 8 | (uint64_t)serial_number.data.word_2[1]);
    ESP_LOGD(SCD40_TAG, "scd40 get serial number:%llu", *pserial_number);
  }
  
  if(r == ESP_OK){
    uint8_t word_crc = calculate_crc(serial_number.data.word_0, sizeof(serial_number.data.word_0));
    ESP_LOGD(SCD40_TAG, "word_0 crc: %x, calculate word_0 crc: %x", serial_number.data.crc_0, word_crc);
    if(word_crc != serial_number.data.crc_0){
      r = ESP_FAIL;
      ESP_LOGE(SCD40_TAG, "scd40 data_0 crc does not match.");
    }
  }
  
  if(r == ESP_OK){
    uint8_t word_crc = calculate_crc(serial_number.data.word_1, sizeof(serial_number.data.word_1));
    ESP_LOGD(SCD40_TAG, "word_1 crc: %x, calculate word_1 crc: %x", serial_number.data.crc_1, word_crc);
    if(word_crc != serial_number.data.crc_1){
      r = ESP_FAIL;
      ESP_LOGE(SCD40_TAG, "scd40 data_1 crc does not match.");
    }
  }
  
  if(r == ESP_OK){
    uint8_t word_crc = calculate_crc(serial_number.data.word_2, sizeof(serial_number.data.word_2));
    ESP_LOGD(SCD40_TAG, "word_2 crc: %x, calculate word_2 crc: %x", serial_number.data.crc_2, word_crc);
    if(word_crc != serial_number.data.crc_2){
      r = ESP_FAIL;
      ESP_LOGE(SCD40_TAG, "scd40 data_2 crc does not match.");
    }
  }
  ESP_LOGI(SCD40_TAG, "scd40 serial number:%llu", *pserial_number);

  return r;
}

esp_err_t SCD40::check_serial_number(){
  esp_err_t r = ESP_OK;
  uint64_t serial_number = 0;
  
  r = get_serial_number(&serial_number);
  if(r != ESP_OK){
      ESP_LOGE(SCD40_TAG, "faild to read serial number");
  }
  return r;
}
  
esp_err_t SCD40::start_periodic_measurement(){
  esp_err_t r = ESP_OK;
  r = send_command(START_PERIODIC_MEASUREMENT_COMMAND);
  if(r != ESP_OK){
    ESP_LOGE(SCD40_TAG, "fail to send periodic measurement command.");
  }
  return r;
}

esp_err_t SCD40::get_sensor_data(uint16_t* pco2, double* ptemperature, double* prelative_humidity) {
  esp_err_t r = ESP_OK;  
  union{
    uint8_t arr[9];
    struct{
      scd40_data_t co2;
      scd40_data_t temperature;
      scd40_data_t relative_humidity;
    }results;
  }measurement_data;
  *pco2 = 0;
  *ptemperature = 0;
  *prelative_humidity = 0;
  if(r == ESP_OK){
    r = read_data(READ_MEASUREMENT_COMMAND, 
        measurement_data.arr, sizeof(measurement_data.arr));
    if(r != ESP_OK) {
      ESP_LOGE(SCD40_TAG, "fail to get sensor data");
      r = stop_periodic_measurement();

      if(r == ESP_OK){
        r = read_data(READ_MEASUREMENT_COMMAND, 
          measurement_data.arr, sizeof(measurement_data.arr));
      }
    }
  }
  if(r == ESP_OK){
    *pco2 = (measurement_data.results.co2.data.value[0] << 8) + measurement_data.results.co2.data.value[1];
    *ptemperature = (double) (175.0 * (((measurement_data.results.temperature.data.value[0] << 8) + measurement_data.results.temperature.data.value[1]) / 65535.0)) - 45.0;
    *prelative_humidity = 100.0 * ((measurement_data.results.relative_humidity.data.value[0] << 8) + measurement_data.results.relative_humidity.data.value[1]) / 65535.0;
  }
  return r;
}

esp_err_t SCD40::get_co2_data(uint16_t* pco2){
  esp_err_t r = ESP_OK;  
  union{
    uint8_t arr[9];
    struct{
      scd40_data_t co2;
      scd40_data_t temperature;
      scd40_data_t relative_humidity;
    }results;
  }measurement_data;
  *pco2 = 0;
  if(r == ESP_OK){
    r = read_data(READ_MEASUREMENT_COMMAND, 
        measurement_data.arr, sizeof(measurement_data.arr));
    if(r != ESP_OK) {
      ESP_LOGE(SCD40_TAG, "fail to get sensor data");
      esp_err_t r2 = ESP_OK; 
      r2 = stop_periodic_measurement();
      if(r2 == ESP_OK){
        r = read_data(READ_MEASUREMENT_COMMAND, 
          measurement_data.arr, sizeof(measurement_data.arr));
      }
    }
  }
  if(r == ESP_OK){
    *pco2 = (measurement_data.results.co2.data.value[0] << 8) + measurement_data.results.co2.data.value[1];
  }
  return r;
 }

esp_err_t SCD40::stop_periodic_measurement(){
  esp_err_t r = ESP_OK;
  r = send_command(STOP_PERIODIC_MEASUREMENT_COMMAND);
  if(r != ESP_OK){
    ESP_LOGE(SCD40_TAG, "failed to transmit stop periodic measurement command ");
  }
  return r;
}

esp_err_t SCD40::set_temperature_offset(float temperature_offset){
  esp_err_t r = ESP_OK;

  uint16_t offset = (temperature_offset * 65536.0) / 175.0;
  ESP_LOGI(SCD40_TAG, "temperature offset is 0x%x", offset);
  scd40_data_t send_data = {
    .arr = {0x00, 0x00, 0x00},
  };
  
  send_data.data.value[0] = offset >> 8; //msb
  send_data.data.value[1] = offset && 0xFF; //lsb
  send_data.data.crc = calculate_crc(send_data.data.value, sizeof(send_data.data.value));
  ESP_LOGI(SCD40_TAG, "temperature offset crc is 0x%x", send_data.data.crc);
  
  r = write_data(SET_TEMPERATURE_OFFSET_COMMAND, send_data.arr, sizeof(send_data.arr));
  if(r != ESP_OK){
    ESP_LOGE(SCD40_TAG, "fail to transmit set temperature offset command.");
  }
  return r;
}

esp_err_t SCD40::get_temperature_offset(float* ptemperature_offset){
  esp_err_t r = ESP_OK;
  
  scd40_data_t read_temperature_offset = {
   .arr = {0x00, 0x00, 0x00},
  }; 

  r = read_data(GET_TEMPERATURE_OFFSET_COMMAND, read_temperature_offset.arr, 
      sizeof(read_temperature_offset.arr));

  if(r != ESP_OK){
    ESP_LOGE(SCD40_TAG, "fail to read temperature offset.");
  }
  if(r == ESP_OK){
    uint8_t expected_crc = calculate_crc(read_temperature_offset.data.value, 
        sizeof(read_temperature_offset.data.value));
    if(expected_crc != read_temperature_offset.data.crc){
      r = ESP_ERR_INVALID_CRC;
      ESP_LOGE(SCD40_TAG, "don't much read crc and expected crc.");
      ESP_LOGE(SCD40_TAG, "read crc: 0x%x, expected crc: 0x%x", read_temperature_offset.data.crc, expected_crc);
    }
  }
  if(r == ESP_OK){
    *ptemperature_offset = round((175 * (((read_temperature_offset.data.value[0] << 8) + 
              read_temperature_offset.data.value[1]) / 65536.0)) * 10.0) / 10.0;
  }

  return r;
}

esp_err_t SCD40::read_data(const uint8_t* pcommand, 
    uint8_t* pread_data_buffer, size_t buffer_size){
  esp_err_t r = ESP_OK;

  if(r == ESP_OK){
    r = pmi2c->read_data(mi2c_device_handle, pcommand, 2, pread_data_buffer, buffer_size);
    if(r != ESP_OK){
      ESP_LOGE(SCD40_TAG, "fail to read data from %x", (pcommand[0] << 8) | pcommand[1]);
    }
  }
  return r;
}

esp_err_t SCD40::write_data(const uint8_t* pcommand, uint8_t* pwrite_data_buffer, size_t buffer_size){
  esp_err_t r = ESP_OK;

  if(r == ESP_OK){
    r = pmi2c->write_data(mi2c_device_handle, pcommand, 2, pwrite_data_buffer, buffer_size);
  }
  return r;
}

esp_err_t SCD40::send_command(const uint8_t* pcommand){
  esp_err_t r = ESP_OK;

  if(r == ESP_OK){
    r = pmi2c->write_data(mi2c_device_handle, pcommand, 2, NULL, 0);
  }

  return r;
} 
