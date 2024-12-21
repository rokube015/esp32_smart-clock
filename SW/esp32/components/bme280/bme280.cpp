#include <memory>

#include "bme280.h"
#include "esp_log.h"

BME280::BME280(){
  esp_log_level_set(BME280_TAG, ESP_LOG_ERROR);
  ESP_LOGI(BME280_TAG, "set BME280_TAG log level: %d", ESP_LOG_DEBUG);
}

esp_err_t BME280::init_i2c(void){
 esp_err_t r = ESP_OK;

 if(r == ESP_OK){
   i2c_device_config_t i2c_device_config = { 
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = DEVICE_ADDRS,
    .scl_speed_hz = 100000,
    .scl_wait_us = 0,
   };
   r = i2c_master_bus_add_device(pmi2c->get_i2c_master_bus_handle(),
       &i2c_device_config, &mi2c_device_handle); 
   if(r != ESP_OK){
     ESP_LOGE(BME280_TAG, "fail to add bme280 to i2c port");
   }
 } 

 return r;
}

esp_err_t BME280::init(
    i2c_base::I2C* pi2c,
    const uint8_t humidity_oversampling,
    const uint8_t temperature_oversampling,
    const uint8_t pressure_oversampling,
    const uint8_t sensor_mode){

  esp_err_t r = ESP_OK;
  
  //add bme280 to pmi2c port
  if(r == ESP_OK){ 
    pmi2c = pi2c;
    r = init_i2c();
    if(r != ESP_OK){ 
      ESP_LOGE(BME280_TAG, "fail to initialize bme280."); 
    }
  }

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

  if(r == ESP_OK){
    r |= write_byte(CONFIG, config_data); 
    r |= get_calibration_data();
    r |= write_byte(CTRL_HUM, mhumidity_oversampling_value);
    r |= write_byte(CTRL_MEAS, ctrl_meas_data);

    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to initialize bme280.");
    }
  }
  return r;
}

uint8_t BME280::get_status(){
  esp_err_t r = ESP_OK; 
  uint8_t read_data {0};
  r = read_byte(STATUS, &read_data);
  if(r != ESP_OK){
    read_data = 0xFF; 
    ESP_LOGE(BME280_TAG, "fail to read STATUS register");
  }
  return read_data;
}

esp_err_t BME280::get_calibration_data(){
  esp_err_t r = ESP_OK;
  int8_t digH4Msb = 0;
  int8_t digH4H5Shared = 0;
  int8_t digH5Msb = 0;
  //============================== Temperature Calibration Data ===========================
  if(r == ESP_OK){
    r = read_uint16_t(0x88, &dig_t1);
    if(r == ESP_OK){ 
      ESP_LOGI(BME280_TAG, "dig_t1: %d", dig_t1);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0x8A, &dig_t2);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_t2: %d", dig_t2);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0x8C, &dig_t3);
    if(r == ESP_OK){ 
      ESP_LOGI(BME280_TAG, "dig_t3: %d", dig_t3);
    }
  }
  //=======================================================================================
  //============================== Pressure Calibration Data ==============================
  if(r == ESP_OK){ 
    r = read_uint16_t(0x8E, &dig_p1);
    if(r == ESP_OK){ 
      ESP_LOGI(BME280_TAG, "dig_p1: %d", dig_p1);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0x90, &dig_p2); 
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_p2: %d", dig_p2);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0x92, &dig_p3);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_p3: %d", dig_p3);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0x94, &dig_p4);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_p4: %d", dig_p4);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0x96, &dig_p5);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_p5: %d", dig_p5);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0x98, &dig_p6);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_p6: %d", dig_p6);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0x9A, &dig_p7);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_p7: %d", dig_p7);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0x9C, &dig_p8);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_p8: %d", dig_p8);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0x9E, &dig_p9);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_p9: %d", dig_p9);
    }
  }
  //=======================================================================================
  //============================== Humidity Calibration Data ==============================
  if(r == ESP_OK){
    r = read_byte(0xA1, &dig_h1);  
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_h1: %d", dig_h1);
    }
  }
  if(r == ESP_OK){
    r = read_int16_t(0xE1, &dig_h2);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_h2: %d", dig_h2);
    }
  }
  if(r == ESP_OK){
    r = read_byte(0xE3, &dig_h3);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_h3: %d", dig_h3);
    }
  }
  if(r == ESP_OK){
    r = read_byte(0xE4, (uint8_t*)&digH4Msb);
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "digH4Msb: %d", digH4Msb);
    }
  }
  if(r == ESP_OK){
    r = read_byte(0xE5, (uint8_t*)(&digH4H5Shared)); // this register hold parts of the values of dig_H4 and dig_h5
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_H4H5Shared: %d", digH4H5Shared);
    }
  }
  if(r == ESP_OK){
    r = read_byte(0xE6, (uint8_t*)(&digH5Msb));
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "digH5Msb: %d", digH5Msb);
    }
  }
  if(r == ESP_OK){
    r = read_byte(0xE7, (uint8_t*)(&dig_h6));
    if(r == ESP_OK){
      ESP_LOGI(BME280_TAG, "dig_h6: %d", dig_h6);
    }
  }
  if(r == ESP_OK){
    dig_h4 = static_cast<int16_t>(digH4Msb << 4 | (digH4H5Shared & 0x0F));        // split and shift the bits appropriately.
    ESP_LOGI(BME280_TAG, "dig_h4: %d", dig_h4);
    dig_h5 = static_cast<int16_t>(digH5Msb << 4 | ((digH4H5Shared & 0xF0) >> 4)); // split and shift the bits appropriately.
    ESP_LOGI(BME280_TAG, "dig_h5: %d", dig_h5);
  }
  //=======================================================================================
  return r;
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
    r = read_data(PRESS_MSB, buff.get(), 8);
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

esp_err_t BME280::get_deviceID(uint8_t* pdeviceID){
  esp_err_t r = ESP_OK;
  
  if(r == ESP_OK){
    r = read_byte(ID, pdeviceID);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to read bme280 device id.");
    }
  }
  return r;
}

esp_err_t BME280::check_deviceID(void){
  esp_err_t r = ESP_OK;
  uint8_t device_id {0};

  if(r == ESP_OK){
    r = get_deviceID(&device_id);
  }
  if(r == ESP_OK){
    if(device_id != 0x60){
      r = ESP_FAIL;
    }
  }
  
  return r;
}

esp_err_t BME280::set_config(const uint8_t config){
  return write_byte(CONFIG, config);
}

esp_err_t BME280::set_config_standby_time(const uint8_t standby){
  // config bits 7, 6, 5  page 30
  esp_err_t r = ESP_OK;
  uint8_t temp_data {0};
  if(r == ESP_OK){
    r = read_byte(CONFIG, &temp_data);
  }
  if(r == ESP_OK){ 
    temp_data &= 0x1F;
    r = write_byte(CONFIG, temp_data | standby);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to set standby time.");
    }
  }
  return r;
}

esp_err_t BME280::set_config_filter(const uint8_t filter){ 
  // config bits 4, 3, 2
  esp_err_t r = ESP_OK; 
  uint8_t temp_data {0};
  if(r == ESP_OK){ 
    r = read_byte(CONFIG, &temp_data);
  }
  if(r == ESP_OK){
    temp_data = temp_data & 0xE3;
    temp_data = temp_data | filter << 2;
    r = write_byte(CONFIG, temp_data);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to set filter configration.");
    }
  }
  return r;
}

esp_err_t BME280::set_ctrl_meas(const uint8_t ctrl_meas){
  esp_err_t r = ESP_OK; 
  mpressure_oversampling_value = 0 | (ctrl_meas & 0xE3);
  mtemperature_oversampling_value = 0 | (ctrl_meas & 0x1F);
  msensor_mode_value = 0 | (ctrl_meas & 0xFC);
  
  r = write_byte(CTRL_MEAS, ctrl_meas);
  return r;
}

esp_err_t BME280::set_temperature_oversampling(const uint8_t temperature_oversampling){
  esp_err_t r = ESP_OK; 
  // ctrl_meas bits 7, 6, 5   page 29
  uint8_t ctrl_meas_data = 0;
  if(r == ESP_OK){ 
    r = read_byte(CTRL_MEAS, &ctrl_meas_data);
  }
  if(r == ESP_OK){ 
    ctrl_meas_data &= 0x1F;
    mtemperature_oversampling_value = temperature_oversampling;
    ctrl_meas_data |= temperature_oversampling;
    r = write_byte(CTRL_MEAS, ctrl_meas_data);
  }
  return r;
}

esp_err_t BME280::set_pressure_oversampling(const uint8_t pressure_oversampling){
  esp_err_t r = ESP_OK; 
  // ctrl_meas bits 4, 3, 2
  uint8_t ctrl_meas_data = 0;
  if(r == ESP_OK){
    r =  read_byte(CTRL_MEAS, &ctrl_meas_data);
  }
  if(r == ESP_OK){ 
    ctrl_meas_data &= 0xE3;
    mpressure_oversampling_value = pressure_oversampling;
    ctrl_meas_data |= mpressure_oversampling_value;
    r = write_byte(CTRL_MEAS, ctrl_meas_data);
  }
  return r;
}

esp_err_t BME280::set_oversampling(const uint8_t temperature_oversampling, const uint8_t pressure_oversampling){
  esp_err_t r = ESP_OK; 
  uint8_t setting_value = 0; 
  mpressure_oversampling_value = 0 | pressure_oversampling;
  mtemperature_oversampling_value = 0 | temperature_oversampling;
  setting_value = mtemperature_oversampling_value | mpressure_oversampling_value | msensor_mode_value;
  r = write_byte(CTRL_MEAS, setting_value);
  return r;
}

esp_err_t BME280::set_mode(const uint8_t mode){
  // ctrl_meas bits 1, 0
  esp_err_t r = ESP_OK;
  uint8_t temp {0};
  if(r == ESP_OK){ 
    r  = read_byte(CTRL_MEAS, &temp);
  }
  if(r == ESP_OK){
    temp &= 0xFC;
    msensor_mode_value = mode;
    ESP_LOGI(BME280_TAG, "set mode:%x", temp|mode);
    r = write_byte(CTRL_MEAS, temp | mode);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to write data.");
    }
  }
  return r;
}

esp_err_t BME280::set_ctrl_hummidity(const int humidity_oversampling){ 
  esp_err_t r = ESP_OK; 
  // ctrl_hum bits 2, 1, 0    page 28
  mhumidity_oversampling_value = humidity_oversampling;
  if(r == ESP_OK){
    r = write_byte(CTRL_HUM, humidity_oversampling);
  }
  return r;
}

esp_err_t BME280::get_all_results(results_data_t* results){
  esp_err_t r = ESP_OK;
  sensor_raw_data_t result_raw{};
  if(r == ESP_OK){
    r = get_sensor_data(&result_raw);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to get sensor data.");
    }
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
  if(r == ESP_OK){
    r = get_sensor_data(&result_raw);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to get sensor data.");
    }
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

  get_all_results(&results); // ToDo: Add error check

  return results.temperature; // compensate_temperature(result_raw.temperature);
}

float BME280::get_pressure(void){
  results_data_t results{};

  get_all_results(&results); // ToDo: Add error check

  return results.pressure;
}

int BME280::get_humidity(void){
  results_data_t results{};

  get_all_results(&results); // ToDo: Add error check

  return results.humidity;
}

bool BME280::check_status_measuring_busy(void){ 
  // check status (0xF3) bit 3
  return ((get_status() == status_measuring) ? true : false);
}

bool BME280::check_imUpdate_busy(void){ 
  // check status (0xF3) bit 0
  return (get_status() == status_update_busy) ? true : false;
}

esp_err_t BME280::reset(void){ // write 0xB6 into reset (0xE0)
  return write_byte(RESET, 0xB6);
}

esp_err_t BME280::write_byte(uint8_t command, const uint8_t value){
  esp_err_t r = ESP_OK;

  if(r == ESP_OK){
    r = pmi2c->write_byte(mi2c_device_handle, &command, 1, value);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to write_data");
    }
  }
  return r;
}

esp_err_t BME280::write_data(const uint8_t command, uint8_t* pwrite_data_buffer, size_t buffer_size){
  esp_err_t r = ESP_OK;

  if(r == ESP_OK){
    r = pmi2c->write_data(mi2c_device_handle, &command, 1, pwrite_data_buffer, buffer_size);
  }

  return r;
}

esp_err_t BME280::read_byte(const uint8_t command, uint8_t* pread_data){
  esp_err_t r = ESP_OK;
  
  if(r == ESP_OK){
    r = pmi2c->read_byte(mi2c_device_handle, &command, 1, pread_data);    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to read data");
    }
  }
  
  return r;
}  

uint8_t BME280::read_byte(const uint8_t command){
  esp_err_t r = ESP_OK;
  uint8_t data = 0;

  if(r == ESP_OK){
    r = pmi2c->read_byte(mi2c_device_handle, &command, 1, &data);
    if(r != ESP_OK){
     ESP_LOGE(BME280_TAG, "fail to read data");
    } 
  }
  return data;
}

esp_err_t BME280::read_int16_t(const uint8_t command, int16_t* pread_data){
  esp_err_t r = ESP_OK;
  uint8_t read_data_buffer[2] {0, 0};

  if(r == ESP_OK){
    pmi2c->read_data(mi2c_device_handle, &command, 1, &read_data_buffer[0], 2);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to read data.");
    }
  }

  if(r == ESP_OK){
    *pread_data = static_cast<int16_t>((read_data_buffer[1] << 8) | read_data_buffer[0]);
  }

  return r;
}

esp_err_t BME280::read_uint16_t(const uint8_t command, uint16_t* pread_data){
  esp_err_t r = ESP_OK;
  uint8_t read_data_buffer[2] {0, 0};

  if(r == ESP_OK){
    pmi2c->read_data(mi2c_device_handle, &command, 1, &read_data_buffer[0], 2);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to read data.");
    }
  }

  if(r == ESP_OK){
    *pread_data = read_data_buffer[1] << 8 | read_data_buffer[0];
  }

  return r;
}

int BME280::read_data(const uint8_t command, 
    uint8_t* pread_data_buffer, size_t buffer_size){
  esp_err_t r = ESP_OK;

  if(r == ESP_OK){
    r = pmi2c->read_data(mi2c_device_handle, &command, 1, 
        pread_data_buffer, buffer_size);
  }
  return r;
}

