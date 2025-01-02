#pragma once

#include "i2c_base.h"

#include "gpio_interface.h"

class AQM0802A{
  private:
    constexpr static const char* AQM0802A_TAG = "aqm00802a";
    i2c_base::I2C* pmi2c;
    GpioInterface::GpioOutput xresetb;
    i2c_master_dev_handle_t mi2c_device_handle;
    // Device settings
    constexpr static uint8_t DEVICE_ADDRS   {0x3e};
    constexpr static uint32_t CLK_SPEED_HZ  {200000};
    // commands
    constexpr static uint8_t CLEAR_DISPLAY      {0x01};
    constexpr static uint8_t RETURN_HOME        {0x02};
    constexpr static uint8_t SET_DDRAM_ADDRESS  {0x80};
    // Boundary value check
    constexpr static uint8_t MIN_ROW_POS {0};
    constexpr static uint8_t MAX_ROW_POS {1};
    constexpr static uint8_t MIN_COL_POS {0};
    constexpr static uint8_t MAX_COL_POS {8};
    
    constexpr static uint8_t ROW_ADDR_OFFSET {0x40};
    // function    
    esp_err_t init_i2c(void);
    esp_err_t send_init_command();    
    esp_err_t send_command(const uint8_t command);
    esp_err_t send_data(const uint8_t data);

  public:
    AQM0802A();
    
    esp_err_t init(i2c_base::I2C* pi2c);
    esp_err_t print_string(const char* pstring);
    esp_err_t clear_display();
    esp_err_t return_cursor_home();
    esp_err_t set_cursor_pos(uint8_t row, uint8_t col);
    esp_err_t execute_hw_reset();
};
