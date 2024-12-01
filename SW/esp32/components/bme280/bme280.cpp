#include <memory>

#include "bme280.h"
#include "esp_log.h"

int BME280::get_status(){
  return read_byte_data(STATUS);
}

esp_err_t BME280::get_calibration_data(){
  //============================== Temperature Calibration Data ===========================
  dig_t1 = static_cast<unsigned short>(read_word_data(0x88));
  ESP_LOGI(BME280_TAG, "dig_t1: %d", dig_t1);
  dig_t2 = static_cast<signed short>(read_word_data(0x8A));
  ESP_LOGI(BME280_TAG, "dig_t2: %d", dig_t2);
  dig_t3 = static_cast<signed short>(read_word_data(0x8C));
  ESP_LOGI(BME280_TAG, "dig_t3: %d", dig_t3);
  //=======================================================================================
  //============================== Pressure Calibration Data ==============================
  dig_p1 = static_cast<unsigned short>(read_word_data(0x8E));
  ESP_LOGI(BME280_TAG, "dig_p1: %d", dig_p1);
  dig_p2 = static_cast<signed short>(read_word_data(0x90));
  ESP_LOGI(BME280_TAG, "dig_p2: %d", dig_p2);
  dig_p3 = static_cast<signed short>(read_word_data(0x92));
  ESP_LOGI(BME280_TAG, "dig_p3: %d", dig_p3);
  dig_p4 = static_cast<signed short>(read_word_data(0x94));
  ESP_LOGI(BME280_TAG, "dig_p4: %d", dig_p4);
  dig_p5 = static_cast<signed short>(read_word_data(0x96));
  ESP_LOGI(BME280_TAG, "dig_p5: %d", dig_p5);
  dig_p6 = static_cast<signed short>(read_word_data(0x98));
  ESP_LOGI(BME280_TAG, "dig_p6: %d", dig_p6);
  dig_p7 = static_cast<signed short>(read_word_data(0x9A));
  ESP_LOGI(BME280_TAG, "dig_p7: %d", dig_p7);
  dig_p8 = static_cast<signed short>(read_word_data(0x9C));
  ESP_LOGI(BME280_TAG, "dig_p8: %d", dig_p8);
  dig_p9 = static_cast<signed short>(read_word_data(0x9E));
  ESP_LOGI(BME280_TAG, "dig_p9: %d", dig_p9);
  //=======================================================================================
  //============================== Humidity Calibration Data ==============================
  dig_h1 = static_cast<unsigned char>(read_byte_data(0xA1));
  ESP_LOGI(BME280_TAG, "dig_h1: %d", dig_h1);
  dig_h2 = static_cast<signed short>(read_word_data(0xE1));
  ESP_LOGI(BME280_TAG, "dig_h2: %d", dig_h2);
  dig_h3 = static_cast<unsigned char>(read_byte_data(0xE3));
  ESP_LOGI(BME280_TAG, "dig_h3: %d", dig_h3);
  int8_t digH4Msb = static_cast<int8_t>(read_byte_data(0xE4));
  ESP_LOGI(BME280_TAG, "digH4Msb: %d", digH4Msb);
  int8_t digH4H5Shared = static_cast<int8_t>(read_byte_data(0xE5)); // this register hold parts of the values of dig_H4 and dig_h5
  ESP_LOGI(BME280_TAG, "dig_H4H5Shared: %d", digH4H5Shared);
  int8_t digH5Msb = static_cast<int8_t>(read_byte_data(0xE6));
  ESP_LOGI(BME280_TAG, "digH5Msb: %d", digH5Msb);
  dig_h6 = static_cast<int8_t>(read_byte_data(0xE7));
  ESP_LOGI(BME280_TAG, "dig_h6: %d", dig_h6);
  dig_h4 = static_cast<signed short>(digH4Msb << 4 | (digH4H5Shared & 0x0F));        // split and shift the bits appropriately.
  ESP_LOGI(BME280_TAG, "dig_h4: %d", dig_h4);
  dig_h5 = static_cast<signed short>(digH5Msb << 4 | ((digH4H5Shared & 0xF0) >> 4)); // split and shift the bits appropriately.
  ESP_LOGI(BME280_TAG, "dig_h5: %d", dig_h5);
  //=======================================================================================

  return 0;
}

esp_err_t BME280::get_sensor_data(sensor_raw_data_t* psensor_result_raw_data){
  esp_err_t r = ESP_OK;

  std::unique_ptr<uint8_t[]> buff = std::make_unique<uint8_t[]>(8);

  if(msensor_mode_value == sensorForcedMode){
    r = set_mode(sensorForcedMode);
    while(check_status_measuring_busy() || check_imUpdate_busy()){
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  
  if(r == ESP_OK){
    r = read_block_data(PRESS_MSB, buff.get(), 8);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to read block data");
    }
  }

  if(r == ESP_OK){
    uint8_t pressure_msb = buff[0];
    uint8_t pressusre_lsb = buff[1];
    uint8_t pressure_xlsb = buff[2];
    uint8_t temperature_msb = buff[3];
    uint8_t temperature_lsb = buff[4];
    uint8_t temperature_xlsb = buff[5];
    uint8_t humidity_msb = buff[6];
    uint8_t humidity_lsb = buff[7];
    psensor_result_raw_data->mtemperature = temperature_msb << 12 | temperature_lsb << 4 | temperature_xlsb >> 4;
    psensor_result_raw_data->mpressure = pressure_msb << 12 | pressusre_lsb << 4 | pressure_xlsb >> 4;
    psensor_result_raw_data->mhumidity = humidity_msb << 8 | humidity_lsb;
    
    ESP_LOGI(BME280_TAG, "raw temperature data: %lu",
        psensor_result_raw_data->mtemperature);
    ESP_LOGI(BME280_TAG, "raw humidity data: %lu",
        psensor_result_raw_data->mhumidity);
    ESP_LOGI(BME280_TAG, "raw pressure data: %lu",
        psensor_result_raw_data->mpressure);
  }

  return r;
}

float BME280::compensate_temperature(const signed long adc_T){
  int32_t var1;
  int32_t var2;
  int32_t temperature;
  int32_t temperature_min = -4000;
  int32_t temperature_max = 8500;

  var1 = (int32_t)(adc_T / 8) - ((uint32_t)dig_t1 * 2);
  var1 = (var1 * ((int32_t)dig_t2)) / 2048;
  var2 = (int32_t)((adc_T / 16) - ((int32_t)dig_t1));
  var2 = (((var2 * var2) / 4096) * ((int32_t)dig_t3)) / 16384;
  t_fine = var1 + var2;
  temperature = (t_fine * 5 + 128) / 256;

  if (temperature < temperature_min){
    temperature = temperature_min;
  }
  else if (temperature > temperature_max){
    temperature = temperature_max;
  }

  return static_cast<float>(temperature) / 100;
}

float BME280::compensate_pressure(const unsigned long adc_P){
  int64_t var1;
  int64_t var2;
  int64_t var3;
  int64_t var4;
  uint32_t pressure;
  uint32_t pressure_min = 3000000;
  uint32_t pressure_max = 11000000;

  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)dig_p6;
  var2 = var2 + ((var1 * (int64_t)dig_p5) * 131072);
  var2 = var2 + (((int64_t)dig_p4) * 34359738368);
  var1 = ((var1 * var1 * (int64_t)dig_p3) / 256) + ((var1 * ((int64_t)dig_p2) * 4096));
  var3 = ((int64_t)1) * 140737488355328;
  var1 = (var3 + var1) * ((int64_t)dig_p1) / 8589934592;

  /* To avoid divide by zero exception */
  if (var1 != 0){
    var4 = 1048576 - adc_P;
    var4 = (((var4 * INT64_C(2147483648)) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_p9) * (var4 / 8192) * (var4 / 8192)) / 33554432;
    var2 = (((int64_t)dig_p8) * var4) / 524288;
    var4 = ((var4 + var1 + var2) / 256) + (((int64_t)dig_p7) * 16);
    pressure = (uint32_t)(((var4 / 2) * 100) / 128);

    if (pressure < pressure_min){
      pressure = pressure_min;
    }
    else if (pressure > pressure_max){
      pressure = pressure_max;
    }
  }
  else{
    pressure = pressure_min;
  }

  return static_cast<float>(pressure) / 100;
}

double BME280::compensate_humidity(const unsigned long adc_H){
  double humidity;

  humidity = static_cast<double>(t_fine) - 76800.0;
  //humidity = (static_cast<double>(adc_H) – (static_cast<double>(dig_h4) * 64.0 + static_cast<double>(dig_h5) / 16384.0 * humidity)) * (static_cast<double>(dig_h2) / 65536.0 * (1.0 + static_cast<double>(dig_h6) / 67108864.0 * humidity * (1.0 + (static_cast<double>(dig_h3)) / 67108864.0 * humidity)));
  //humidity = humidity * (1.0 – static_cast<double>(dig_h1) * humidity / 524288.0);
  humidity = ((double)adc_H - (((double)dig_h4) * 64.0 + ((double)dig_h5) / 16384.0 * humidity)) * (((double)dig_h2) / 65536.0 * (1.0 + ((double)dig_h6) / 67108864.0 * humidity * (1.0 + ((double)dig_h3) / 67108864.0 * humidity)));
  humidity = humidity * (1.0 - ((double)dig_h1) * humidity / 524288.0);
  
  ESP_LOGI(BME280_TAG, "calculate compensate humidity: %f", humidity);
  if (humidity > 100.0){
    humidity = 100.0;
  }
  else if(humidity < 0.0){
    humidity = 0.0;
  }

  return humidity;
}

BME280::BME280(){
  esp_log_level_set(BME280_TAG, ESP_LOG_DEBUG);
  ESP_LOGI(BME280_TAG, "set BME280_TAG error level: %d", ESP_LOG_DEBUG);
}

esp_err_t BME280::init(const uint8_t humidity_oversampling,
    const uint8_t temperature_oversampling,
    const uint8_t pressure_oversampling,
    const uint8_t sensor_mode){
  
  esp_err_t r = ESP_OK;

  mhumidity_oversampling_value = humidity_oversampling;
  ESP_LOGI(BME280_TAG, "set humidity_oversampling:%x", humidity_oversampling); 
  mpressure_oversampling_value = pressure_oversampling;
  ESP_LOGI(BME280_TAG, "set pressure_oversampling:%x", pressure_oversampling); 
  mtemperature_oversampling_value = temperature_oversampling;
  ESP_LOGI(BME280_TAG, "set temperature_oversampling:%x", temperature_oversampling); 
  msensor_mode_value = sensor_mode;
  ESP_LOGI(BME280_TAG, "set sensor mode:%x", sensor_mode); 
 
  uint8_t config_data = t_standby_500 | filter_off | spi3w_disable; 
  uint8_t ctrl_meas_data = mpressure_oversampling_value | mtemperature_oversampling_value | msensor_mode_value;

  ESP_LOGI(BME280_TAG, "set CONFIG register to %x", config_data); 
  ESP_LOGI(BME280_TAG, "ser CTRL_MEAS register to %x", ctrl_meas_data);
  r |= write_byte_data(CONFIG, config_data); 
  r |= get_calibration_data();
  r |= write_byte_data(CTRL_HUM, mhumidity_oversampling_value);
  r |= write_byte_data(CTRL_MEAS, ctrl_meas_data);
  
  if(r != ESP_OK){
    ESP_LOGE(BME280_TAG, "fail to initialize bme280.");
  }

  return r;
}

int BME280::get_deviceID(){
  return read_byte_data(ID);
}

esp_err_t BME280::set_config(const uint8_t config){
  return write_byte_data(CONFIG, config);
}

esp_err_t BME280::set_config_standby_time(const uint8_t standby){
  // config bits 7, 6, 5  page 30
  esp_err_t r = ESP_OK;
  uint8_t temp = read_byte_data(CONFIG) & 0b00011111;
  r = write_byte_data(CONFIG, temp | standby);
  if(r != ESP_OK){
    ESP_LOGE(BME280_TAG, "fail to set standby time.");
  }

  return r;
}

esp_err_t BME280::set_config_filter(const uint8_t filter){ 
  // config bits 4, 3, 2
  esp_err_t r = ESP_OK; 
  uint8_t temp = read_byte_data(CONFIG);
  temp = temp & 0b11100011;
  temp = temp | filter << 2;
  r = write_byte_data(CONFIG, temp);
  
  if(r != ESP_OK){
    ESP_LOGE(BME280_TAG, "fail to set filter configration.");
  }
  
  return r;
}

esp_err_t BME280::set_ctrl_meas(const uint8_t ctrl_meas){
  mpressure_oversampling_value = 0 | (ctrl_meas & 0b11100011);
  mtemperature_oversampling_value = 0 | (ctrl_meas & 0b00011111);
  msensor_mode_value = 0 | (ctrl_meas & 0b11111100);

  return write_byte_data(CTRL_MEAS, ctrl_meas);
}

esp_err_t BME280::set_temperature_oversampling(const uint8_t temperature_oversampling){
  // ctrl_meas bits 7, 6, 5   page 29
  uint8_t temperature = read_byte_data(CTRL_MEAS) & 0b00011111;
  mtemperature_oversampling_value = temperature_oversampling;

  return write_byte_data(CTRL_MEAS, temperature | temperature_oversampling);
}

esp_err_t BME280::set_pressure_oversampling(const uint8_t pressure_oversampling){
  // ctrl_meas bits 4, 3, 2
  uint8_t temp = read_byte_data(CTRL_MEAS) & 0b11100011;
  mpressure_oversampling_value = pressure_oversampling;

  return write_byte_data(CTRL_MEAS, temp |mpressure_oversampling_value);
}

esp_err_t BME280::set_oversampling(const uint8_t temperature_oversampling, const uint8_t pressure_oversampling){
  mpressure_oversampling_value = 0 | pressure_oversampling;
  mtemperature_oversampling_value = 0 | temperature_oversampling;

  return write_byte_data(CTRL_MEAS, mtemperature_oversampling_value | mpressure_oversampling_value | msensor_mode_value);
}

esp_err_t BME280::set_mode(const uint8_t mode){
  // ctrl_meas bits 1, 0
  esp_err_t r = ESP_OK;
  uint8_t temp = read_byte_data(CTRL_MEAS) & 0xFC;
  msensor_mode_value = mode;
  ESP_LOGI(BME280_TAG, "set sensor mode: %x", msensor_mode_value);
  r = write_byte_data(CTRL_MEAS, temp | mode);
  ESP_LOGI(BME280_TAG, "set mode:%x", temp|mode);
  return r;
}

esp_err_t BME280::set_ctrl_hummidity(const int humidity_oversampling){ 
  // ctrl_hum bits 2, 1, 0    page 28
  mhumidity_oversampling_value = humidity_oversampling;

  return write_byte_data(CTRL_HUM, humidity_oversampling);
}

esp_err_t BME280::get_all_results(results_data_t* results){
  esp_err_t r = ESP_OK;
  sensor_raw_data_t result_raw{};

  r = get_sensor_data(&result_raw);
  if(r != ESP_OK){
    ESP_LOGE(BME280_TAG, "fail to get sensor data.");
  }

  if(r == ESP_OK){
    results->temperature = compensate_temperature(result_raw.mtemperature);
    results->humidity = compensate_humidity(result_raw.mhumidity);
    results->pressure = compensate_pressure(result_raw.mpressure);
  }

  return r;
}

esp_err_t BME280::get_all_results(float* ptemperature, double* phumidity, float *ppressure){
  esp_err_t r = ESP_OK;
  sensor_raw_data_t result_raw{};

  r = get_sensor_data(&result_raw);
  
  if(r != ESP_OK){
    ESP_LOGE(BME280_TAG, "fail to get sensor data.");
  }
  
  if(r == ESP_OK){
    *ptemperature = compensate_temperature(result_raw.mtemperature);
    *phumidity = compensate_humidity(result_raw.mhumidity);
    *ppressure = compensate_pressure(result_raw.mpressure);
    ESP_LOGI(BME280_TAG, "compensate_temperature: %f",
        *ptemperature);
    ESP_LOGI(BME280_TAG, "compensate_humidity: %f",
        *phumidity);
    ESP_LOGI(BME280_TAG, "compensate_pressure: %f",
        *ppressure);
  }

  return r;
}

float BME280::get_temperature(void){
  // Preferable to use get_all_results()
  results_data_t results{};

  get_all_results(&results);

  return results.temperature; // compensate_temperature(result_raw.temperature);
}

float BME280::get_pressure(void){
  results_data_t results{};

  get_all_results(&results);

  return results.pressure;
}

int BME280::get_humidity(void){
  results_data_t results{};

  get_all_results(&results);

  return results.humidity;
}

bool BME280::check_status_measuring_busy(void){ // check status (0xF3) bit 3
  return ((read_byte_data(STATUS) & 8) == 8) ? true : false;
}

bool BME280::check_imUpdate_busy(void){ // check status (0xF3) bit 0
  return ((read_byte_data(STATUS) & 1) == 1) ? true : false;
}

esp_err_t BME280::reset(void){ // write 0xB6 into reset (0xE0)
  return write_byte_data(RESET, 0xB6);
}

esp_err_t BME280::write_byte_data(const uint8_t reg, const uint8_t value){
  return pmi2c->write_register(mdevice_address, reg, value);
}

int BME280::read_byte_data(const uint8_t reg){
  return pmi2c->read_register(mdevice_address, reg);
}

int BME280::read_word_data(const uint8_t reg)
{
  uint8_t buff[2];
  pmi2c->read_register_multiple_bytes(mdevice_address, reg, buff, 2);
  return buff[1] << 8 | buff[0];
}

esp_err_t BME280::read_block_data(const uint8_t reg, uint8_t *buf, const int length)
{
  return pmi2c->read_register_multiple_bytes(mdevice_address, reg, buf, length);
}

esp_err_t BME280::init_i2c(i2c_base::I2C* pi2c, const uint8_t device_address){
  esp_err_t r = ESP_OK;
  
  if(r == ESP_OK){ 
    pmi2c = pi2c;
    mdevice_address = device_address;
  }

  return r;
}
