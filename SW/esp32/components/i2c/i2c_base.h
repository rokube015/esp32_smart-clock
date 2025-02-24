/**
 * @file i2c_base.h
 * @brief Declaration of the base I2C class for ESP32.
 *
 * This file declares the I2C class which provides basic functionalities for I2C communication
 * on the ESP32 platform.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/i2c_master.h"
#include "esp_intr_alloc.h"

namespace i2c_base {

  /**
   * @brief Manages I2C communication on the ESP32.
   *
   * This class handles the initialization and basic read/write operations on the I2C bus.
   */
  class I2C {
    private:
      /**
       * @brief Tag for logging purposes.
       */
      constexpr static const char* I2C_BASE_TAG = "i2c_base";

      /**
       * @brief GPIO number used for the SDA pin.
       */
      constexpr static gpio_num_t SDA_PIN {GPIO_NUM_2};

      /**
       * @brief GPIO number used for the SCL pin.
       */
      constexpr static gpio_num_t SCL_PIN {GPIO_NUM_1};

      /**
       * @brief I2C timeout in milliseconds.
       */
      constexpr static int I2C_TIMEOUT {1000};

      /**
       * @brief Semaphore for exclusive access to the I2C port.
       */
      SemaphoreHandle_t i2c_port_semaphore {NULL};

      /**
       * @brief Handle to the I2C master bus.
       */
      i2c_master_bus_handle_t mi2c_bus_handle;

      /**
       * @brief Configuration for the I2C master bus.
       */
      i2c_master_bus_config_t mi2c_bus_config;

      /**
       * @brief Slave device address.
       */
      uint16_t mslave_addrs{};

      /**
       * @brief I2C port number.
       */
      i2c_port_num_t mport{};

      /**
       * @brief I2C mode (master or slave).
       */
      i2c_mode_t mmode{};

      /**
       * @brief Acquires the semaphore for the I2C port.
       *
       * @return esp_err_t Error code.
       */
      esp_err_t take_i2c_port_semaphore();

      /**
       * @brief Releases the semaphore for the I2C port.
       *
       * @return esp_err_t Error code.
       */
      esp_err_t release_i2c_port_semaphore();

    public:
      /**
       * @brief Constructor.
       *
       * Initializes the I2C instance with the specified port and mode.
       *
       * @param port I2C port number (default is -1).
       * @param mode I2C mode (default is I2C_MODE_MASTER).
       */
      I2C(i2c_port_num_t port = -1, i2c_mode_t mode = I2C_MODE_MASTER);

      /**
       * @brief Destructor.
       *
       * Releases any allocated resources.
       */
      ~I2C();

      /**
       * @brief Initializes the I2C bus.
       *
       * Configures the I2C bus and enables pull-up resistors if specified.
       *
       * @param pullup_enable Set to true to enable pull-up resistors.
       * @return esp_err_t Error code.
       */
      esp_err_t init(bool pullup_enable = true);

      /**
       * @brief Retrieves the I2C master bus handle.
       *
       * @return i2c_master_bus_handle_t Handle to the I2C master bus.
       */
      i2c_master_bus_handle_t get_i2c_master_bus_handle();

      /**
       * @brief Reads a single byte.
       *
       * Sends the specified command and reads one byte from the device.
       *
       * @param dev_handle Device handle.
       * @param pcommand Pointer to the command data.
       * @param command_size Size of the command.
       * @param pread_data_buffer Buffer to store the read byte.
       * @return esp_err_t Error code.
       */
      esp_err_t read_byte(i2c_master_dev_handle_t dev_handle,
          const uint8_t* pcommand, size_t command_size, uint8_t* pread_data_buffer);

      /**
       * @brief Writes a single byte.
       *
       * Sends the specified command and writes one byte to the device.
       *
       * @param dev_handle Device handle.
       * @param pcommand Pointer to the command data.
       * @param command_size Size of the command.
       * @param write_data Byte to write.
       * @return esp_err_t Error code.
       */
      esp_err_t write_byte(i2c_master_dev_handle_t dev_handle,
          const uint8_t* pcommand, size_t command_size, const uint8_t write_data);

      /**
       * @brief Reads multiple bytes.
       *
       * Sends the specified command and reads multiple bytes into the provided buffer.
       *
       * @param dev_handle Device handle.
       * @param pcommand Pointer to the command data.
       * @param command_size Size of the command.
       * @param pread_data_buffer Buffer to store the read data.
       * @param read_buffer_size Size of the read buffer.
       * @return esp_err_t Error code.
       */
      esp_err_t read_data(i2c_master_dev_handle_t dev_handle,
          const uint8_t* pcommand, size_t command_size,
          uint8_t* pread_data_buffer, size_t read_buffer_size);

      /**
       * @brief Writes multiple bytes.
       *
       * Sends the specified command and writes multiple bytes from the provided buffer.
       *
       * @param dev_handle Device handle.
       * @param pcommand Pointer to the command data.
       * @param command_size Size of the command.
       * @param pwrite_data_buffer Buffer containing the data to write.
       * @param write_buffer_size Size of the write buffer.
       * @return esp_err_t Error code.
       */
      esp_err_t write_data(i2c_master_dev_handle_t dev_handle,
          const uint8_t* pcommand, size_t command_size,
          const uint8_t* pwrite_data_buffer, size_t write_buffer_size);
  };
}
