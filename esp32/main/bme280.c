#include "bme280.h"
#include "i2c_base.h"

static const char* BME280_TAG = "bme280";

i2c_master_dev_handle_t bme280_handle;

uint16_t dig_T1;
int16_t dig_T2;
int16_t dig_T3;
uint16_t dig_P1;
int16_t dig_P2;
int16_t dig_P3;
int16_t dig_P4;
int16_t dig_P5;
int16_t dig_P6;
int16_t dig_P7;
int16_t dig_P8;
int16_t dig_P9;
int8_t  dig_H1;
int16_t dig_H2;
int8_t  dig_H3;
int16_t dig_H4;
int16_t dig_H5;
int8_t  dig_H6;
int32_t t_fine;

esp_err_t read_bme280_compensation_parameter();
int32_t get_bme280_calibration_temperature(int32_t raw_temperature);
uint32_t get_bme280_calibration_pressure(int32_t raw_pressure);
uint32_t get_bme280_calibration_humidity(int32_t raw_humidity);

esp_err_t init_bme280(){
  esp_err_t r = ESP_OK;
  esp_log_level_set(BME280_TAG, ESP_LOG_DEBUG);
  
  i2c_device_config_t i2c_dev_conf = {    
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x76,
    .scl_speed_hz = 100000,
  };
  
  if(r == ESP_OK){
    r = i2c_master_bus_add_device(bus_handle, &i2c_dev_conf, &bme280_handle);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to add device");
    }
  }

  if(r == ESP_OK){
    r = check_bme280_chip_id();
  }
  
  uint8_t osrs_t = 1;
  uint8_t osrs_p = 1;
  uint8_t osrs_h = 1;
  uint8_t mode = 3;
  uint8_t t_sb = 5;
  uint8_t filter = 0;
  uint8_t spi3w_en = 0;

  uint8_t ctrl_meas_reg = (osrs_t << 5) | (osrs_p << 2) | mode;
  uint8_t config_reg = (t_sb << 5) | (filter << 2) | spi3w_en;
  uint8_t ctrl_hum_reg =osrs_h;

  uint8_t send_data[6];
  send_data[0] = 0xF2;
  send_data[1] = ctrl_hum_reg;
  send_data[2] = 0xF4;
  send_data[3] = ctrl_meas_reg;
  send_data[4] = 0xF5;
  send_data[5] = config_reg;

  if(r == ESP_OK){
    r = i2c_master_transmit(bme280_handle, send_data, sizeof(send_data), -1);
    ESP_LOGI(BME280_TAG, "send bme280 configure command");
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG,"fail to send bme280 configure data");
    }
  }

  if(r == ESP_OK){
   r = read_bme280_compensation_parameter();
   ESP_LOGI(BME280_TAG, "read bme280 compensation parameter.");
   if(r != ESP_OK){
     ESP_LOGE(BME280_TAG, "fail to read bme280 compensation parameter.");
   }
  }
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

esp_err_t read_bme280_compensation_parameter(){
  uint8_t r = ESP_OK;

  uint8_t data25[25];
  uint8_t data7[8];
  
  uint8_t calib_dig_T1_addrs = 0x88;
  uint8_t calib_dig_H2_addrs = 0xE1;

  if(r == ESP_OK){
    r = i2c_master_transmit_receive(bme280_handle, &calib_dig_T1_addrs, 1, data25, sizeof(data25), -1);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to read bme280 calibration data");
    }
  }

  if(r == ESP_OK){
    r = i2c_master_transmit_receive(bme280_handle, &calib_dig_H2_addrs, 1, data7, sizeof(data7), -1);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to read bme280 calibration data");
    }
  }
  
  if(r == ESP_OK){
    dig_T1 = (data25[1] << 8) | data25[0];
    dig_T2 = (data25[3] << 8) | data25[2];
    dig_T3 = (data25[5] << 8) | data25[4];
    dig_P1 = (data25[7] << 8) | data25[6];
    dig_P2 = (data25[9] << 8) | data25[8];
    dig_P3 = (data25[11]<< 8) | data25[10];
    dig_P4 = (data25[13]<< 8) | data25[12];
    dig_P5 = (data25[15]<< 8) | data25[14];
    dig_P6 = (data25[17]<< 8) | data25[16];
    dig_P7 = (data25[19]<< 8) | data25[18];
    dig_P8 = (data25[21]<< 8) | data25[20];
    dig_P9 = (data25[23]<< 8) | data25[22];
    dig_H1 = data25[24];
    dig_H2 = (data7[1]<< 8) | data7[0];
    dig_H3 = data7[2];
    dig_H4 = (data7[3]<< 4) | (0x0F & data7[4]);
    dig_H5 = (data7[5] << 4) | ((data7[4] >> 4) & 0x0F); 
    dig_H6 = data7[6];
  }

  return r;
}

esp_err_t get_bme280_data(bme280_value_t* bme280){
  esp_err_t r = ESP_OK;
  uint8_t data[8];
  uint8_t data_addrs = 0xF7;
  int32_t raw_temperature = 0;
  int32_t raw_pressure = 0;
  int32_t raw_humidity = 0;

  if(r == ESP_OK){
    r = i2c_master_transmit_receive(bme280_handle, &data_addrs, 1, data, sizeof(data), -1);
    if(r != ESP_OK){
      ESP_LOGE(BME280_TAG, "fail to read data");
    }
  }

  if(r == ESP_OK){
    raw_pressure = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    raw_temperature = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    raw_humidity = (data[6] << 8) | data[7];

    bme280->temperature = get_bme280_calibration_temperature(raw_temperature) /100.0;
    bme280->pressure = get_bme280_calibration_pressure(raw_pressure) / 100.0;
    bme280->humidity = get_bme280_calibration_humidity(raw_humidity) / 1024.0;
  }

  return r;
}

int32_t get_bme280_calibration_temperature(int32_t raw_temperature){
  int32_t var1, var2, T;
  var1 = ((((raw_temperature >> 3) - ((int32_t)dig_T1<<1))) * ((int32_t)dig_T2)) >> 11;
  var2 = (((((raw_temperature >> 4) - ((int32_t)dig_T1)) * ((raw_temperature>>4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
  t_fine = var1 + var2;
  T = (t_fine * 5 + 128) >> 8;
  return T;
}

uint32_t get_bme280_calibration_pressure(int32_t raw_pressure){
  int32_t var1, var2;
  uint32_t P;
  var1 = (((int32_t)t_fine)>>1) - (int32_t)64000;
  var2 = (((var1>>2) * (var1>>2)) >> 11) * ((int32_t)dig_P6);
  var2 = var2 + ((var1*((int32_t)dig_P5))<<1);
  var2 = (var2>>2)+(((int32_t)dig_P4)<<16);
  var1 = (((dig_P3 * (((var1>>2)*(var1>>2)) >> 13)) >>3) + ((((int32_t)dig_P2) * var1)>>1))>>18;
  var1 = ((((32768+var1))*((int32_t)dig_P1))>>15);
  if (var1 == 0)
  {
    return 0;
  }    
  P = (((uint32_t)(((int32_t)1048576)-raw_pressure)-(var2>>12)))*3125;
  if(P<0x80000000)
  {
    P = (P << 1) / ((uint32_t) var1);   
  }
  else
  {
    P = (P / (uint32_t)var1) * 2;    
  }
  var1 = (((int32_t)dig_P9) * ((int32_t)(((P>>3) * (P>>3))>>13)))>>12;
  var2 = (((int32_t)(P>>2)) * ((int32_t)dig_P8))>>13;
  P = (uint32_t)((int32_t)P + ((var1 + var2 + dig_P7) >> 4));
  return P;
}

uint32_t get_bme280_calibration_humidity(int32_t raw_humidity){
  int32_t v_x1;

  v_x1 = (t_fine - ((int32_t)76800));
  v_x1 = (((((raw_humidity << 14) -(((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1)) + 
          ((int32_t)16384)) >> 15) * (((((((v_x1 * ((int32_t)dig_H6)) >> 10) * 
          (((v_x1 * ((int32_t)dig_H3)) >> 11) + ((int32_t) 32768))) >> 10) + ((int32_t)2097152)) * 
          ((int32_t) dig_H2) + 8192) >> 14));
  v_x1 = (v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
  v_x1 = (v_x1 < 0 ? 0 : v_x1);
  v_x1 = (v_x1 > 419430400 ? 419430400 : v_x1);
  return (uint32_t)(v_x1 >> 12);   
}


