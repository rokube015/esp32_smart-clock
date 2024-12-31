#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2c_base.h"

class SCD40{
  private:
    constexpr static const char* SCD40_TAG = "scd40"; 
    i2c_base::I2C* pmi2c;
    
    i2c_master_dev_handle_t mi2c_device_handle;
    // Device settings
    constexpr static uint8_t DEVICE_ADDRS = 0x62;
    constexpr static uint32_t CLK_SPEED_HZ = 100000;
    // commands
    constexpr static uint8_t GET_SERIAL_NUMBER_COMMAND[2]           {0x36, 0x82};
    constexpr static uint8_t START_PERIODIC_MEASUREMENT_COMMAND[2]  {0x21, 0xb1};
    constexpr static uint8_t READ_MEASUREMENT_COMMAND[2]            {0xec, 0x05};
    constexpr static uint8_t STOP_PERIODIC_MEASUREMENT_COMMAND[2]   {0x3f, 0x86};
    constexpr static uint8_t SET_TEMPERATURE_OFFSET_COMMAND[2]      {0x24, 0x1d}; 
    constexpr static uint8_t GET_TEMPERATURE_OFFSET_COMMAND[2]      {0x23, 0x18};
    // Settings
    typedef struct{
      uint16_t co2;
      double temperature;
      double relative_humidity;
    }results_data_t;

    typedef union{
      uint8_t arr[3];
      struct{
        uint8_t value[2];
        uint8_t crc;
      }data;
     }scd40_data_t;
    
    TaskHandle_t task_handle = NULL;    
    uint16_t co2;

    esp_err_t init_i2c(void);
    uint8_t calculate_crc(const uint8_t* data, uint16_t byte_size);

    esp_err_t set_temperature_offset(float temperature_offset);
    esp_err_t get_temperature_offset(float* ptemperature_offset);
    
    esp_err_t read_data(const uint8_t* pcommand, uint8_t* pread_data_buffer, size_t buffer_size);
    esp_err_t write_data(const uint8_t* pcommand, uint8_t* pwrite_data_buffer, size_t buffer_size);
    esp_err_t send_command(const uint8_t* command);

    void measure_co2_task();
    static void get_measure_co2_task_entry_point(void* arg);

  public:
    SCD40();
    
    esp_err_t init(i2c_base::I2C* pi2c);    
    //esp_err_t Close();
    esp_err_t get_serial_number(uint64_t* pserial_number);
    esp_err_t check_serial_number();
    // Wait for 5 seconds before sending the next command
    esp_err_t start_periodic_measurement();
    esp_err_t stop_periodic_measurement();
    esp_err_t get_sensor_data(uint16_t* pco2, double* ptemperature, double* prelative_humidity);
    esp_err_t get_co2_data(uint16_t* pco2);
    esp_err_t get_co2(uint16_t* pco2);

    esp_err_t create_task(const char* pname, uint16_t stack_size, UBaseType_t task_priority);
};
