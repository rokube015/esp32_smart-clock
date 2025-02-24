/**
 * @file bme280.h
 * @brief BME280 sensor driver for ESP32.
 *
 * This file contains the class declaration for interfacing with the BME280
 * temperature, humidity, and pressure sensor.
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2c_base.h"

/**
 * @brief Class to interface with the BME280 sensor.
 *
 * Provides functions to initialize the sensor, read temperature, pressure,
 * and humidity, and process sensor calibration data.
 */
class BME280{
  private:
    /**
     * @brief Debug tag for logging.
     */
    constexpr static const char* BME280_TAG = "bme280";

    /**
     * @brief Pointer to the I2C communication instance.
     */
    i2c_base::I2C* pmi2c;

    /**
     * @brief I2C device handle for BME280.
     */
    i2c_master_dev_handle_t mi2c_device_handle;

    // ==========================
    // Sensor Device Settings
    // ==========================

    /**
     * @brief I2C device address for BME280.
     */
    constexpr static uint8_t DEVICE_ADDRS {0x76};
    /**
     * @brief I2C clock speed in Hz.
     */
    constexpr static uint32_t CLK_SPEED_HZ {800000};
    /**
     * @brief BME280 register addresses.
     */
    constexpr static uint8_t HUM_LSB    {0xFE};
    constexpr static uint8_t HUM_MSB    {0xFD};
    constexpr static uint8_t TEMP_XLSB  {0xFC};
    constexpr static uint8_t TEMP_LSB   {0xFB};
    constexpr static uint8_t TEMP_MSB   {0xFA};
    constexpr static uint8_t PRESS_XLSB {0xF9};
    constexpr static uint8_t PRESS_LSB  {0xF8};
    constexpr static uint8_t PRESS_MSB  {0xF7};
    constexpr static uint8_t CONFIG     {0xF5};
    constexpr static uint8_t CTRL_MEAS  {0xF4};
    constexpr static uint8_t STATUS     {0xF3};
    constexpr static uint8_t CTRL_HUM   {0xF2};
    constexpr static uint8_t RESET      {0xE0};
    constexpr static uint8_t ID         {0xD0};
    // ==========================
    // Sensor Configuration Settings
    // ==========================

    constexpr static uint8_t pressureSensorDisable = 0x00 << 2;

    /**
     * @brief Oversampling settings for pressure measurement.
     */
    constexpr static uint8_t pressureOversamplingX1 = 0x01 << 2;
    constexpr static uint8_t pressureOversamplingX2 = 0x02 << 2;
    constexpr static uint8_t pressureOversamplingX4 = 0x03 << 2;
    constexpr static uint8_t pressureOversamplingX8 = 0x04 << 2;
    constexpr static uint8_t pressureOversamplingX16 = 0x05 << 2;
    /**
     * @brief Oversampling settings for temperature measurement.
     */
    constexpr static uint8_t temperatureSensorDisable = 0x00 << 5;
    constexpr static uint8_t temperatureOversamplingX1 = 0x01 << 5;
    constexpr static uint8_t temperatureOversamplingX2 = 0x02 << 5;
    constexpr static uint8_t temperatureOversamplingX4 = 0x03 << 5;
    constexpr static uint8_t temperatureOversamplingX8 = 0x04 << 5;
    constexpr static uint8_t temperatureOversamplingX16 = 0x05 << 5;
    /**
     * @brief Sensor operating modes.
     */
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
    /**
     * @brief Oversampling settings for humidity measurement.
     */
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
    /**
     * @brief Sensor mode configuration.
     */   

    uint8_t msensor_mode_value = sensorForcedMode;              // Default to forced mode
    /**
     * @brief Structure to hold raw sensor data.
     */  
    typedef struct {
      long mtemperature = 0;
      unsigned long mhumidity = 0;
      unsigned long mpressure = 0;
    }sensor_raw_data_t;

    uint8_t mhumidity_oversampling_value = humidityOversamplingX1;    // Default to 1X over sampling
    uint8_t mpressure_oversampling_value = pressureOversamplingX1;    // Default to 1X over sampling
    uint8_t mtemperature_oversampling_value = temperatureOversamplingX1; // Default to 1X over sampling
  // ==========================
  // Calibration Data
  // ==========================                                                               
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

    /**
     * @brief Task handle for measurement task.
     */
    TaskHandle_t task_handle {NULL};
    /**
     * @brief Queue handle for results buffer.
     */
    QueueHandle_t results_buffer {NULL};
    /**
     * @brief Size of results buffer.
     */ 
    UBaseType_t results_buffer_size {1};
    /**
     * @brief Initializes I2C communication.
     *
     * @return ESP_OK if successful, error code otherwise.
     */
    esp_err_t init_i2c(void);

    /**
     * @brief Retrieves sensor status.
     *
     * @return Sensor status register value.
     */
    uint8_t get_status();

    /**
     * @brief Reads the sensor calibration data.
     *
     * @return ESP_OK if successful, error code otherwise.
     */
    esp_err_t get_calibration_data();

    /**
     * @brief Retrieves raw sensor data.
     *
     * @param sensor_result_row_data Pointer to store raw sensor data.
     * @return ESP_OK if successful, error code otherwise.
     */
    esp_err_t get_sensor_data(sensor_raw_data_t* sensor_result_row_data);

    /**
     * @brief Compensates temperature data.
     *
     * @param adc_T Raw temperature data.
     * @return Compensated temperature data.
     */  
    float compensate_temperature(const signed long adc_T);

    /**
     * @brief Compensates pressure data.
     *
     * @param adc_P Raw pressure data.
     * @return Compensated pressure data.
     */ 
    float compensate_pressure(const unsigned long adc_P);

    /**
     * @brief Compensates humidity data.
     *
     * @param adc_H Raw humidity data.
     * @return Compensated humidity data.
     */
    double compensate_humidity(const unsigned long adc_H);
    /**
     * @brief Writes a byte to the specified register.
     *
     * @param command Register address.
     * @param value Data to write.
     * @return ESP_OK if successful, error code otherwise.
     */
    esp_err_t write_byte(const uint8_t command, const uint8_t value);

    /**
     * @brief Reads a byte from the specified register.
     *
     * @param command Register address.
     * @param pread_data Pointer to store read data.
     * @return ESP_OK if successful, error code otherwise.
     */
    esp_err_t read_byte(const uint8_t command, uint8_t* pread_data);

    /**
     * @brief Reads a byte from the specified register.
     *
     * @param command Register address.
     * @return Read data.
     */
    uint8_t read_byte(const uint8_t command);
    /**
     * @brief Reads a 16-bit signed integer from the specified register.
     *
     * @param command Register address.
     * @param pread_data Pointer to store read data.
     * @return ESP_OK if successful, error code otherwise.
     */ 
    esp_err_t read_int16_t(const uint8_t command, int16_t* pread_data);
    /**
     * @brief Reads a 16-bit unsigned integer from the specified register.
     *
     * @param command Register address.
     * @param pread_data Pointer to store read data.
     * @return ESP_OK if successful, error code otherwise.
     */ 
    esp_err_t read_uint16_t(const uint8_t command, uint16_t* pread_data);
    /**
     * @brief Reads a 16-bit signed integer from the specified register.
     *
     * @param command Register address.
     * @return Read data.
     */ 
    esp_err_t read_data(const uint8_t command, uint8_t* pread_data_buffer, size_t buffer_size);

    /**
     * @brief Writes data to the specified register.
     *
     * @param command Register address.
     * @param pwrite_data_buffer Pointer to data to write.
     * @param buffer_size Size of data to write.
     * @return ESP_OK if successful, error code otherwise.
     */ 
    esp_err_t write_data(const uint8_t command, uint8_t* pwrite_data_buffer, size_t buffer_size);
    /**
     * @brief Measurement task for reading sensor data periodically.
     */   
    void measure_task();
    /**
     * @brief Entry point for measurement task.
     *
     * @param arg Task argument.
     */
    static void get_measure_task_entry_point(void* arg);

  public:
    /**
     * @brief Structure to hold sensor data.
     */ 
    typedef struct{
      float temperature = 0.0;
      double humidity = 0;
      float pressure = 0.0;
    }results_data_t;

    /**
     * @brief Constructor.
     *
     * Initializes internal parameters.
     */
    results_data_t results_data;
    /**
     * @brief Constructor.
     *
     * Initializes internal parameters.
     */   
    BME280();

    /**
     * @brief Destructor.
     *
     * Cleans up resources.
     */ 
    ~BME280();

    /**
     * @brief Initializes the BME280 sensor.
     *
     * @param pi2c Pointer to the I2C communication instance.
     * @param humidity_oversampling Humidity oversampling setting.
     * @param temperature_oversampling Temperature oversampling setting.
     * @param pressure_oversampling Pressure oversampling setting.
     * @param sensor_mode Sensor mode setting.
     * @return ESP_OK if successful, error code otherwise.
     */
    esp_err_t init(
        i2c_base::I2C* pi2c, 
        const uint8_t humidity_oversampling = humidityOversamplingX1,
        const uint8_t temperature_oversampling = temperatureOversamplingX1,
        const uint8_t pressure_oversampling = pressureOversamplingX1,
        const uint8_t sensor_mode = sensorForcedMode);
    //esp_err_t Close(void);
    /**
     * @brief Retrieves the results buffer.
     *
     * @return Results buffer.
     */ 
    QueueHandle_t get_results_buffer(); 
    /**
     * @brief Creates a task to read sensor data periodically.
     *
     * @param pname Task name.
     * @param stack_size Task stack size.
     * @param task_priority Task priority.
     * @return ESP_OK if successful, error code otherwise.
     */ 
    esp_err_t create_task(const char* pname, uint16_t stack_size, UBaseType_t task_priority);

    /**
     * @brief Starts the measurement task.
     */
    void notify_measurement_start();

    /**
     * @breif Get the device ID
     *
     * @param pdeviceID Pointer to store device ID.
     * @return ESP_OK if successful, error code otherwise.
     */
    esp_err_t get_deviceID(uint8_t* pdeviceID);
    /**
     * @brief Check the device ID
     *
     * @return ESP_OK if successful, error code otherwise.
     */ 
    esp_err_t check_deviceID(void);
    /**
     * @brief Set the sensor configuration.
     *
     * @param config Configuration value.
     * @return ESP_OK if successful, error code otherwise.
     */ 
    esp_err_t set_config(const uint8_t config);
    /**
     * @brief Set the standby time.
     *
     * @param standby Standby time value.
     * @return ESP_OK if successful, error code otherwise.
     */ 
    esp_err_t set_config_standby_time(const uint8_t standby);   // config bits 7, 6, 5  page 30
    /**
     * @brief Set the filter value.
     *
     * @param filter Filter value.
     * @return ESP_OK if successful, error code otherwise.
     */ 
    esp_err_t set_config_filter(const uint8_t filter);      // config bits 4, 3, 2
    /**
     * @brief Set control measurement. 
     *   
     *   @param ctrlMeas Control measurement value.
     *   @return ESP_OK if successful, error code otherwise.
     */
    esp_err_t set_ctrl_meas(const uint8_t ctrlMeas);
    /**
     * @brief Set temperature oversampling. 
     *
     *  @param tempOversampling Temperature oversampling value.
     *  @return ESP_OK if successful, error code otherwise.
     */
    esp_err_t set_temperature_oversampling(const uint8_t tempOversampling);     // ctrl_meas bits 7, 6, 5   page 29
    /**
     * @brief Set pressure oversampling. 
     *
     * @param pressureOversampling Pressure oversampling value.
     * @return ESP_OK if successful, error code otherwise.
     */
    esp_err_t set_pressure_oversampling(const uint8_t pressureOversampling);    // ctrl_meas bits 4, 3, 2
   /**
    * @brief Set humidity oversampling. 
    *
    * @param humidityOversampling Humidity oversampling value.
    * @return ESP_OK if successful, error code otherwise.
    */
    esp_err_t set_oversampling(const uint8_t temperature_oversampling, const uint8_t pressure_oversampling);
   /**
    * @brief Set sensor mode. 
    *   
    *   @param mode Sensor mode value.
    *   @return ESP_OK if successful, error code otherwise.
    */
    esp_err_t set_mode(const uint8_t mode);                                    // ctrl_meas bits 1, 0
   /**
    * @brief Set control humidity. 
    *
    * @param humididty_oversampling Humidity oversampling value.
    * @return ESP_OK if successful, error code otherwise.
    */
    esp_err_t set_ctrl_hummidity(const int humididty_oversampling);                    // ctrl_hum bits 2, 1, 0    page 28
   /** 
    * @brief Update sensor data. 
    *
    * @return ESP_OK if successful, error code otherwise.
    */
    esp_err_t update_sensor_data(); 
   /** 
    * @brief Get sensor data.
    *   
    *   @param results Pointer to store sensor data.
    *   @return ESP_OK if successful, error code otherwise.
    */
    esp_err_t get_all_results(results_data_t *results);
    esp_err_t get_all_results(float *temperature, double *humidity, float *pressure);
    float get_temperature(void);    
    float get_pressure(void);       
    int get_humidity(void);       
    esp_err_t get_temperature(float* ptemperature);    
    esp_err_t get_pressure(float* ppressure);       
    esp_err_t get_humidity(double* humidity);       

    bool check_status_measuring_busy(void); // check status (0xF3) bit 3
    bool check_imUpdate_busy(void);        // check status (0xF3) bit 0
    esp_err_t reset(void);                // write 0xB6 into reset (0xE0)
};
