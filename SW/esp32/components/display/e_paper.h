#pragma once
#include "driver/spi_master.h"
#include "esp_system.h"
#include "esp_log.h"

#include "gpio_interface.h"

class EPAPER4IN26{
  public:  
    enum class state_e{
      NOT_INITIALIZED,
      INITIALIZED,
      RUNNING,
      SLEEP
    };

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
    constexpr static uint16_t DISPLAY_RESOLUTION_HEIGHT  {480};
    constexpr static uint16_t DISPLAY_RESOLUTION_WIDTH   {800};
    constexpr static uint16_t DISPLAY_ROW_LENGTH         {DISPLAY_RESOLUTION_WIDTH / 8};
    constexpr static int      DISPLAY_DISP_BYTES         {DISPLAY_ROW_LENGTH * DISPLAY_RESOLUTION_HEIGHT};
    
    // E-paper command
    constexpr static uint8_t DRIVER_OUTPUT_CONTROL_COMMAND          {0x01};
    constexpr static uint8_t DEEP_SLEEP_MODE_COMMAND                {0x10};
    constexpr static uint8_t SW_RESET_COMMAND                       {0x12};
    constexpr static uint8_t TEMPERATURE_SENSOR_CONTROL_COMMAND     {0x18};
    constexpr static uint8_t BOOSTER_SOFT_START_CONTROL_COMMAND     {0x0C};
    constexpr static uint8_t MASTER_ACTIVATION_COMMAND              {0x20};
    constexpr static uint8_t DISPLAY_UPDATE_CONTROL_1_COMMAND       {0x21};
    constexpr static uint8_t DISPLAY_UPDATE_CONTROL_2_COMMAND       {0x22}; 
    constexpr static uint8_t WRITE_RAM_0x24_COMMAND                 {0x24};
    constexpr static uint8_t WRITE_RAM_0x26_COMMAND                 {0x26};
    constexpr static uint8_t BORDER_WAVEFORM_CONTROL_COMMAND        {0x3C};
    constexpr static uint8_t DATA_ENTRY_MODE_COMMAND                {0x11};
    constexpr static uint8_t SET_X_START_END_POSITION_COMMAND       {0x44};
    constexpr static uint8_t SET_Y_START_END_POSITION_COMMAND       {0x45};
    constexpr static uint8_t SET_X_ADDRESS_COUNTER_COMMAND          {0x4E};
    constexpr static uint8_t SET_Y_ADDRESS_COUNTER_COMMAND          {0x4F}; 
    // E-paper settings
    constexpr static uint8_t BORDER_WAVEFORM_CONTROL_SETTING        {0x01};
    constexpr static uint8_t USE_INTERNAL_TEMPERATURE_SENSOR        {0x80}; 
    constexpr static uint8_t SOFT_START_CONTROL_SETTING[5]          {0xAE, 0xC7, 0xC3, 0xC0, 0x80};
    constexpr static uint8_t DRIVER_OUTPUT_CONTROL_SETTING[3]       {(DISPLAY_RESOLUTION_HEIGHT-1)%256, (DISPLAY_RESOLUTION_HEIGHT-1)/256, 0x02};
    constexpr static uint8_t DATA_ENTRY_MODE_SETTING                {0x01};
    // valiables
    constexpr static uint16_t MAX_SPI_TARANSFER_SIZE   {16*1000}; 
    spi_device_handle_t spi_handle;
    static state_e mstate; 
    //class
    GpioInterface::GpioOutput dc_pin;
    GpioInterface::GpioOutput rst_pin;  
    GpioInterface::GpioInput  busy_pin;
    
    //function
    esp_err_t init_spi_bus();
    esp_err_t init_gpio();
    esp_err_t send_command(const uint8_t addr, const uint8_t* pdata_buffer, size_t buffer_size);
    esp_err_t send_frame(const uint8_t* pdata_buffer, size_t buffer_size);
    uint8_t  is_busy(); // Returns: 0: Host side can send data to driver. 1: Driver is busy.
    esp_err_t wait_until_ready();
    esp_err_t set_windows(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);
  public:
    DMA_ATTR static uint8_t transffer_buffer[DISPLAY_DISP_BYTES];
    
    EPAPER4IN26();
    uint16_t get_display_resolution_height(){return DISPLAY_RESOLUTION_HEIGHT;}
    uint16_t get_display_resolution_width(){return DISPLAY_RESOLUTION_WIDTH;}
    uint16_t get_display_row_length(){return DISPLAY_ROW_LENGTH;}
    int      get_display_bytes(){return DISPLAY_DISP_BYTES;}        
    state_e  get_state(){return mstate;};
    esp_err_t init();
    esp_err_t init_epaper(); 
    esp_err_t execute_hw_reset(); 
    esp_err_t turn_on_display();
    esp_err_t display(const uint8_t* pblack_image, size_t black_image_size);
    esp_err_t display_number(const uint8_t *pimage, uint8_t num);
    esp_err_t clear_screen();
    esp_err_t set_sleep_mode();
    esp_err_t set_cursur(uint16_t x_position, uint16_t y_position);
};

class EPAPER3IN52{
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
    DMA_ATTR static uint8_t transffer_buffer[DISPLAY_DISP_BYTES];
    
    EPAPER3IN52();
    uint16_t get_display_resolution_height(){return DISPLAY_RESOLUTION_HEIGHT;}
    uint16_t get_display_resolution_width(){return DISPLAY_RESOLUTION_WIDTH;}
    uint16_t get_display_row_length(){return DISPLAY_ROW_LENGTH;}
    int      get_display_bytes(){return DISPLAY_DISP_BYTES;}        
    esp_err_t init();
    esp_err_t execute_hw_reset(); 
    esp_err_t turn_on_display();
    esp_err_t display(const uint8_t* pblack_image, size_t black_image_size, 
        const uint8_t* pred_image, size_t red_image_size);
    esp_err_t display_number(const uint8_t *pimage, uint8_t num);
    esp_err_t display_black();
    esp_err_t clear_screen();
    esp_err_t set_sleep_mode();
};
