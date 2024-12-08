#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2c_base.h"

class BME280{
  private:
    constexpr static char* BME280_TAG = "bme280";
    i2c_base::I2C* pmi2c;
    i2c_master_dev_handle_t mi2c_device_handle;
    
    // Device addrs
    constexpr static uint8_t DEVICE_ADDRS = 0x76;
    
    // Registers
    constexpr static uint8_t HUM_LSB = 0xFE;
    constexpr static uint8_t HUM_MSB = 0xFD;
    constexpr static uint8_t TEMP_XLSB = 0xFC;
    constexpr static uint8_t TEMP_LSB = 0xFB;
    constexpr static uint8_t TEMP_MSB = 0xFA;
    constexpr static uint8_t PRESS_XLSB = 0xF9;
    constexpr static uint8_t PRESS_LSB = 0xF8;
    constexpr static uint8_t PRESS_MSB = 0xF7;
    constexpr static uint8_t CONFIG = 0xF5;
    constexpr static uint8_t CTRL_MEAS = 0xF4;
    constexpr static uint8_t STATUS = 0xF3;
    constexpr static uint8_t CTRL_HUM = 0xF2;
    constexpr static uint8_t RESET = 0xE0;
    constexpr static uint8_t ID = 0xD0;

    // Settings
    constexpr static uint8_t pressureSensorDisable = 0x00 << 2;
    constexpr static uint8_t pressureOversamplingX1 = 0x01 << 2;
    constexpr static uint8_t pressureOversamplingX2 = 0x02 << 2;
    constexpr static uint8_t pressureOversamplingX4 = 0x03 << 2;
    constexpr static uint8_t pressureOversamplingX8 = 0x04 << 2;
    constexpr static uint8_t pressureOversamplingX16 = 0x05 << 2;
    constexpr static uint8_t temperatureSensorDisable = 0x00 << 5;
    constexpr static uint8_t temperatureOversamplingX1 = 0x01 << 5;
    constexpr static uint8_t temperatureOversamplingX2 = 0x02 << 5;
    constexpr static uint8_t temperatureOversamplingX4 = 0x03 << 5;
    constexpr static uint8_t temperatureOversamplingX8 = 0x04 << 5;
    constexpr static uint8_t temperatureOversamplingX16 = 0x05 << 5;
    constexpr static uint8_t sensorSleepMode = 0x00;
    constexpr static uint8_t sensorForcedMode = 0x01;
    constexpr static uint8_t sensorNormalMode = 0x03;

    constexpr static uint8_t configStandby0_5ms = 0x00 << 5;
    constexpr static uint8_t configStandby62_5ms = 0x01 << 5;
    constexpr static uint8_t configStandby125ms = 0x02 << 5;
    constexpr static uint8_t configStandby250ms = 0x03 << 5;
    constexpr static uint8_t configStandby500ms = 0x04 << 5;
    constexpr static uint8_t configStandby1000ms = 0x05 << 5;
    constexpr static uint8_t configStandby10ms = 0x06 << 5;
    constexpr static uint8_t configStandby20ms = 0x07 << 5;
    constexpr static uint8_t configFilterOff = 0x00 << 2;
    constexpr static uint8_t configFilter2 = 0x01 << 2;
    constexpr static uint8_t configFilter4 = 0x02 << 2;
    constexpr static uint8_t configFilter8 = 0x03 << 2;
    constexpr static uint8_t configFilter16 = 0x04 << 2;

    constexpr static uint8_t humiditySensorDisable = 0x00;
    constexpr static uint8_t humidityOversamplingX1 = 0x01;
    constexpr static uint8_t humidityOversamplingX2 = 0x02;
    constexpr static uint8_t humidityOversamplingX4 = 0x03;
    constexpr static uint8_t humidityOversamplingX8 = 0x04;
    constexpr static uint8_t humidityOversamplingX16 = 0x05;
    constexpr static uint8_t spi3w_disable = 0x00;
    constexpr static uint8_t t_standby_0_5 = 0x00;
    constexpr static uint8_t t_standby_62_5 = 0x20;
    constexpr static uint8_t t_stabdby_125 = 0x40;
    constexpr static uint8_t t_standby_250 = 0x60;
    constexpr static uint8_t t_standby_500 = 0x80;
    constexpr static uint8_t filter_off = 0x00;
    constexpr static uint8_t status_measuring = 0x08;
    constexpr static uint8_t status_measured = 0x00;
    constexpr static uint8_t status_update_busy = 0x01;
    constexpr static uint8_t status_update_done = 0x00;

    uint8_t msensor_mode_value = sensorForcedMode;              // Default to forced mode
    
    typedef struct {
      long mtemperature = 0;
      unsigned long mhumidity = 0;
      unsigned long mpressure = 0;
    }sensor_raw_data_t;

    uint8_t mhumidity_oversampling_value = humidityOversamplingX1;    // Default to 1X over sampling
    uint8_t mpressure_oversampling_value = pressureOversamplingX1;    // Default to 1X over sampling
    uint8_t mtemperature_oversampling_value = temperatureOversamplingX1; // Default to 1X over sampling
                                                                
    // Calibration Data
    uint16_t  dig_t1 = 0;
    int16_t   dig_t2 = 0;
    int16_t   dig_t3 = 0;
    int32_t   t_fine = 0;
    uint16_t  dig_p1 = 0;
    int16_t   dig_p2 = 0;
    int16_t   dig_p3 = 0;
    int16_t   dig_p4 = 0;
    int16_t   dig_p5 = 0;
    int16_t   dig_p6 = 0;
    int16_t   dig_p7 = 0;
    int16_t   dig_p8 = 0;
    int16_t   dig_p9 = 0;
    uint8_t   dig_h1 = 0;
    int16_t   dig_h2 = 0;
    uint8_t   dig_h3 = 0;
    int16_t   dig_h4 = 0;
    int16_t   dig_h5 = 0;
    int8_t    dig_h6 = 0;

    esp_err_t init_i2c(void);
    uint8_t get_status();
    esp_err_t get_calibration_data();
    esp_err_t get_sensor_data(sensor_raw_data_t* sensor_result_row_data);
    float compensate_temperature(const signed long adc_T);
    float compensate_pressure(const unsigned long adc_P);
    double compensate_humidity(const unsigned long adc_H);

    esp_err_t write_byte(const uint8_t command, const uint8_t value);
    esp_err_t read_byte(const uint8_t command, uint8_t* pread_data);
    uint8_t read_byte(const uint8_t command);
    esp_err_t read_int16_t(const uint8_t command, int16_t* pread_data);
    esp_err_t read_uint16_t(const uint8_t command, uint16_t* pread_data);
    esp_err_t read_data(const uint8_t command, uint8_t* pread_data_buffer, size_t buffer_size);
    esp_err_t write_data(const uint8_t command, uint8_t* pwrite_data_buffer, size_t buffer_size);

  public:
    typedef struct{
      float temperature = 0.0;
      double humidity = 0;
      float pressure = 0.0;
    }results_data_t;

    results_data_t results_data;
    
    BME280();

    esp_err_t init(
        i2c_base::I2C* pi2c, 
        const uint8_t humidity_oversampling = humidityOversamplingX1,
        const uint8_t temperature_oversampling = temperatureOversamplingX1,
        const uint8_t pressure_oversampling = pressureOversamplingX1,
        const uint8_t sensor_mode = sensorForcedMode);
    //esp_err_t Close(void);
    esp_err_t get_deviceID(uint8_t* pdeviceID);
    esp_err_t check_deviceID(void);
    esp_err_t set_config(const uint8_t config);
    esp_err_t set_config_standby_time(const uint8_t standby);   // config bits 7, 6, 5  page 30
    esp_err_t set_config_filter(const uint8_t filter);      // config bits 4, 3, 2
    esp_err_t set_ctrl_meas(const uint8_t ctrlMeas);
    esp_err_t set_temperature_oversampling(const uint8_t tempOversampling);     // ctrl_meas bits 7, 6, 5   page 29
    esp_err_t set_pressure_oversampling(const uint8_t pressureOversampling);    // ctrl_meas bits 4, 3, 2
    esp_err_t set_oversampling(const uint8_t temperature_oversampling, const uint8_t pressure_oversampling);
    esp_err_t set_mode(const uint8_t mode);                                    // ctrl_meas bits 1, 0
    esp_err_t set_ctrl_hummidity(const int humididty_oversampling);                    // ctrl_hum bits 2, 1, 0    page 28
    esp_err_t get_all_results(results_data_t *results);
    esp_err_t get_all_results(float *temperature, double *humidity, float *pressure);
    float get_temperature(void);    // Preferable to use GetAllResults()
    float get_pressure(void);       
    int get_humidity(void);       
    bool check_status_measuring_busy(void); // check status (0xF3) bit 3
    bool check_imUpdate_busy(void);        // check status (0xF3) bit 0
    esp_err_t reset(void);                // write 0xB6 into reset (0xE0)
};
