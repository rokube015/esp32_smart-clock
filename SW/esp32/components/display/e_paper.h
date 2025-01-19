#pragma once
#include "driver/spi_master.h"
#include "esp_system.h"
#include "esp_log.h"

#include "gpio_interface.h"


class EPAPER{
  private:
    constexpr static const char* EPAPER_TAG = "e-papaer";
    
    // periperal settings
    constexpr static spi_host_device_t EPAPER_SPI_HOST {SPI3_HOST};

    constexpr static gpio_num_t SPI_SCK_PIN      {GPIO_NUM_14}; // SCL
    constexpr static gpio_num_t SPI_MOSI_PIN     {GPIO_NUM_13}; // SDA
    constexpr static gpio_num_t SPI_CS_PIN       {GPIO_NUM_21}; // output
    constexpr static gpio_num_t EPAPER_RST_PIN   {GPIO_NUM_35}; // output
    constexpr static gpio_num_t EPAPER_DC_PIN    {GPIO_NUM_48}; // output
    constexpr static gpio_num_t EPAPER_BUSY_PIN  {GPIO_NUM_47}; // input

    constexpr static int SPI_CLOCK_SPEED  {5 * 1000 * 1000};

    // display settings
    constexpr static uint16_t DISPLAY_RESOLUTION_HEIGHT  {360};
    constexpr static uint16_t DISPLAY_RESOLUTION_WIDTH   {240};
    constexpr static uint16_t DISPLAY_ROW_LENGTH         {DISPLAY_RESOLUTION_WIDTH / 8};
    constexpr static int      DISPLAY_DISP_BYTES         {DISPLAY_ROW_LENGTH * DISPLAY_RESOLUTION_HEIGHT};
    
    // E-paper command
    constexpr static uint8_t PANEL_SETTING_COMMAND              {0x00};
    constexpr static uint8_t POWER_ON_COMMAND                   {0x04};
    constexpr static uint8_t BOOSTER_SOFT_START_COMMAND         {0x06};
    constexpr static uint8_t DISPLAY_START_TRANSMISSION_1       {0x10};
    constexpr static uint8_t DISPLAY_START_TRANSMISSION_2       {0x13};
    constexpr static uint8_t SET_DISPLAY_RESOLUTION_COMMAND     {0x61};
    constexpr static uint8_t STARTING_DATA_TRANSMISSION_COMMAND {0x06};
    constexpr static uint8_t DISPLAY_REFRESH_COMMAND            {0x12};

    // E-paper settings
    constexpr static uint8_t PANEL_SETTINGS[2]                      {0x03, 0x0D};
    constexpr static uint8_t DISPLAY_RESOLUTION_SETTINGS[3]         {0xF0, 0x01, 0x68}; // 240x360
    constexpr static uint8_t STARTING_DATA_TRANSMISSION_SETTINGS[3] {0x2F, 0x2F, 0x2E};

    // valiables
    spi_device_handle_t spi_handle;
    DMA_ATTR static uint8_t transffer_buffer[DISPLAY_DISP_BYTES];
    
    //class
    GpioInterface::GpioOutput dc_pin;
    GpioInterface::GpioOutput rst_pin;  
    GpioInterface::GpioInput  busy_pin;
    
    //function
    esp_err_t init_spi_bus();
    esp_err_t init_gpio();
    esp_err_t init_epaper(); 
    esp_err_t send_command(const uint8_t addr, const uint8_t* pdata_buffer, size_t buffer_size);
    esp_err_t send_frame(const uint8_t* pdata_buffer, size_t buffer_size);
    uint8_t  is_busy(); // Returns: 0: Host side can send data to driver. 1: Driver is busy.
    esp_err_t wait_until_ready();
  public:
    EPAPER();
    const uint16_t get_display_resolution_height(){return DISPLAY_RESOLUTION_HEIGHT;}
    const uint16_t get_display_resolution_width(){return DISPLAY_RESOLUTION_WIDTH;}
    const uint16_t get_display_row_length(){return DISPLAY_ROW_LENGTH;}
    const int      get_display_bytes(){return DISPLAY_DISP_BYTES;}        
    esp_err_t init();
    esp_err_t execute_hw_reset(); 
    esp_err_t turn_on_display();
    esp_err_t display(const uint8_t* pblack_image, size_t black_image_size, 
        const uint8_t* pred_image, size_t red_image_size);
    esp_err_t display_number(const uint8_t *pimage, uint8_t num);
    esp_err_t clear_screen();
    esp_err_t display_black();
    esp_err_t set_sleep_mode();
};
