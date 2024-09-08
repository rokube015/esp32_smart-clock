#include "scd40.h"

static const char* SCD40_TAG = "scd40_tag";

i2c_master_dev_handle_t dev_handle;

esp_err_t init_scd40(){
  esp_err_t r = ESP_OK;
  esp_log_level_set(SCD40_TAG, ESP_LOG_DEBUG);
  
  i2c_master_bus_config_t i2c_bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = PORT_NUMBER,
    .scl_io_num = GPIO_NUM_22,
    .sda_io_num = GPIO_NUM_21,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true, 
  };
  i2c_master_bus_handle_t bus_handle;

  r = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
  ESP_ERROR_CHECK(r);

  i2c_device_config_t i2c_dev_conf = {    
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x62,
    .scl_speed_hz = 100000,
  };

  r = i2c_master_bus_add_device(bus_handle, &i2c_dev_conf, &dev_handle);
  ESP_ERROR_CHECK(r);

  return r;
}

uint8_t calculate_scd40_crc(const uint8_t* data, uint16_t byte_size){
  uint16_t current_byte = 0;
  uint8_t crc = 0xff;
  const uint8_t CRC8_POLYNOMIAL = 0x31;
  uint8_t crc_bit = 0;
  /* calculates 8-Bit checksum with given polynomial */
  for(current_byte = 0; current_byte < byte_size; ++current_byte){
    crc ^= (data[current_byte]);
    for (crc_bit = 8; crc_bit > 0; --crc_bit){
      if (crc & 0x80)
        crc = (crc << 1) ^ CRC8_POLYNOMIAL;
      else
        crc = (crc << 1);
    }
  }
  ESP_LOGI(SCD40_TAG, "Calculate CRC = %x", crc);
  return crc;
}

esp_err_t get_scd40_serial_number(uint64_t* pserial_number){
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

  uint8_t serial_number_get_command[2] = {0x36, 0x82};
  r = i2c_master_transmit(dev_handle, serial_number_get_command, sizeof(serial_number_get_command), -1);
  ESP_ERROR_CHECK(r);
  if(r != ESP_OK){
    ESP_LOGE(SCD40_TAG, "failed transmit command data with status code: %s", esp_err_to_name(r));
    return r;
  }

  r = i2c_master_receive(dev_handle, serial_number.arr, sizeof(serial_number.arr), -1);
  ESP_ERROR_CHECK(r);
  if(r != ESP_OK) {
    ESP_LOGE(SCD40_TAG, "get_serial_number failed with status code: %s", esp_err_to_name(r));
    return r;
  }

  *pserial_number =  ((uint64_t)serial_number.data.word_0[0] << 40 | (uint64_t)serial_number.data.word_0[1] << 32) |
    ((uint64_t)serial_number.data.word_1[0] << 24 | (uint64_t)serial_number.data.word_0[1] << 16) |
    ((uint64_t)serial_number.data.word_2[0] << 8 | (uint64_t)serial_number.data.word_2[1]);
  ESP_LOGI(SCD40_TAG, "scd40 serial number:%llu", *pserial_number);

  uint8_t word_crc = calculate_scd40_crc(serial_number.data.word_0, sizeof(serial_number.data.word_0));
  ESP_LOGI(SCD40_TAG, "word_0 crc: %x, calculate word_0 crc: %x", serial_number.data.crc_0, word_crc);
  if(word_crc != serial_number.data.crc_0){
    r = ESP_FAIL;
    ESP_LOGE(SCD40_TAG, "scd40 data_0 crc does not match.");
  }

  word_crc = calculate_scd40_crc(serial_number.data.word_1, sizeof(serial_number.data.word_1));
  ESP_LOGI(SCD40_TAG, "word_1 crc: %x, calculate word_1 crc: %x", serial_number.data.crc_1, word_crc);
  if(word_crc != serial_number.data.crc_1){
    r = ESP_FAIL;
    ESP_LOGE(SCD40_TAG, "scd40 data_1 crc does not match.");
  }

  word_crc = calculate_scd40_crc(serial_number.data.word_2, sizeof(serial_number.data.word_2));
  ESP_LOGI(SCD40_TAG, "word_2 crc: %x, calculate word_2 crc: %x", serial_number.data.crc_2, word_crc);
  if(word_crc != serial_number.data.crc_2){
    r = ESP_FAIL;
    ESP_LOGE(SCD40_TAG, "scd40 data_2 crc does not match.");
  }

  return r;
}

esp_err_t start_scd40_periodic_measurement(){
  esp_err_t r = ESP_OK;
  uint8_t periodic_measurement_start_command[2] = {0x21, 0xb1};
  r = i2c_master_transmit(dev_handle, periodic_measurement_start_command, sizeof(periodic_measurement_start_command), -1);
  ESP_ERROR_CHECK(r);
  if(r != ESP_OK){
    ESP_LOGE(SCD40_TAG, "failed transmit command data with status code: %s", esp_err_to_name(r));
  }
  return r;
}

esp_err_t get_scd40_sensor_data(scd40_value_t* scd40_value) {
  esp_err_t r = ESP_OK;  
  union{
    uint8_t arr[9];
    struct{
      uint8_t co2[2];
      uint8_t co2_crc;
      uint8_t temperature[2];
      uint8_t temperature_crc;
      uint8_t relative_humidity[2];
      uint8_t relative_humidity_crc;
    }data;
  }measurement_data;

  uint8_t measurement_read_command[2] = {0xec, 0x05};
  //if(!scd4x_get_data_ready_status()) {
  //    return ESP_FAIL;
  // }
  r = i2c_master_transmit(dev_handle, measurement_read_command, sizeof(measurement_read_command), -1);
  ESP_ERROR_CHECK(r);
  if(r != ESP_OK){
    ESP_LOGE(SCD40_TAG, "failed transmit command data with status code: %s", esp_err_to_name(r));
    return r;
  }
  r = i2c_master_receive(dev_handle, measurement_data.arr, sizeof(measurement_data.arr), -1);
  ESP_ERROR_CHECK(r);
  if(r != ESP_OK) {
    ESP_LOGE(SCD40_TAG, "get_serial_number failed with status code: %s", esp_err_to_name(r));
    return r;
  }
  scd40_value->co2 = (measurement_data.data.co2[0] << 8) + measurement_data.data.co2[1];
  scd40_value->temperature = (double) (175.0 * (((measurement_data.data.temperature[0] << 8) + measurement_data.data.temperature[1]) / 65535.0)) - 45.0;
  scd40_value->relative_humidity = 100.0 * ((measurement_data.data.relative_humidity[0] << 8) + measurement_data.data.relative_humidity[1]) / 65535.0;
  return r;
}

esp_err_t stop_scd40_periodic_measurement() {
 esp_err_t r = ESP_OK;
  uint8_t periodic_measurement_stop_command[2] = {0x3f, 0x86};
  r = i2c_master_transmit(dev_handle, periodic_measurement_stop_command, sizeof(periodic_measurement_stop_command), -1);
  ESP_ERROR_CHECK(r);
  if(r != ESP_OK){
    ESP_LOGE(SCD40_TAG, "failed transmit command data with status code: %s", esp_err_to_name(r));
  }
  return r;
}

esp_err_t set_scd40_temperature_offset(double temperature_offset){
  esp_err_t r = ESP_OK;

  uint16_t offset = (temperature_offset * 65536.0) / 175.0;
  
  scd40_data_t scd40_temperature_data = {
    .data = {0x00, 0x00},
    .crc = 0x00
  };
  
  uint8_t set_offset_temperature_command[2] = {0x24, 0x1d}; 
  scd40_temperature_data.data[0] = offset >> 8; //msb
  scd40_temperature_data.data[1] = offset && 0xFF; //lsb
  scd40_temperature_data.crc = calculate_scd40_crc(scd40_temperature_data.data, sizeof(scd40_temperature_data));

  uint8_t transmit_data[5] = {
    set_offset_temperature_command[0],
    set_offset_temperature_command[1],
    scd40_temperature_data.data[0],
    scd40_temperature_data.data[1],
    scd40_temperature_data.crc,
  };
  
  r = i2c_master_transmit(dev_handle, transmit_data, sizeof(transmit_data), -1);
  ESP_ERROR_CHECK(r);
  if(r != ESP_OK){
    ESP_LOGE(SCD40_TAG, "failed transmit command data with status code: %s", esp_err_to_name(r));
  }
  
  return r;
}
