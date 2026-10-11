#pragma once
#include "esp_err.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include <cstdio>
#include <mutex>

class SD_CARD{
  private:
    std::mutex file_mutex;
    sdmmc_card_t* mounted_card {nullptr};
    bool spi_bus_owned {false};
    constexpr static const char* SD_CARD_TAG = "sd_card";
    
    constexpr static char MOUNT_POINT[]= "/sdcard";
    constexpr static int SPI_MISO_PIN         = GPIO_NUM_39;
    constexpr static int SPI_MOSI_PIN         = GPIO_NUM_41;
    constexpr static int SPI_CLK_PIN          = GPIO_NUM_40;
    constexpr static gpio_num_t SPI_CS_PIN    = GPIO_NUM_42;
   
    constexpr static int MAX_SDCARD_LINE_CHAR_SIZE = 512;
  public:
    SD_CARD();

    esp_err_t init();
    void deinit();

    // mode 'h': append a header only when the file is empty or new.
    esp_err_t write_data(const char* pfile_path, size_t file_path_size, char* data, char mode);

    esp_err_t read_data(const char* path);
    esp_err_t open_sensor_log(FILE** file, long* size);
    esp_err_t read_sensor_log(FILE* file, char* buffer, size_t size);
    void close_sensor_log(FILE* file);
};
