#include <string.h>
#include "sd_card.h"

static const char* SD_CARD_TAG = "sd_card_tag";

esp_err_t write_sd_card_file(const char* path, char* data, char mode){
  ESP_LOGI(SD_CARD_TAG, "Opening file %s", path);
  FILE* pFile;
  if(mode == 'a'){
    pFile = fopen(path, "a");
  }
  else{
    pFile = fopen(path, "w");
  }
  if(pFile == NULL){
    ESP_LOGE(SD_CARD_TAG, "Faild to open file for writing");
    return ESP_FAIL;
  }
  fprintf(pFile, data);
  fclose(pFile);
  ESP_LOGI(SD_CARD_TAG, "File written");

  return ESP_OK;
}

esp_err_t read_sd_card_file(const char* path){
  ESP_LOGI(SD_CARD_TAG, "Reading file %s", path);
  FILE* pFile = fopen(path, "r");
  if(pFile == NULL){
    ESP_LOGE(SD_CARD_TAG, "Failed to open file for reading");
    return ESP_FAIL;
  }
  char line[MAX_SDCARD_LINE_CHAR_SIZE];
  fgets(line, sizeof(line), pFile);
  fclose(pFile);

  // strip newline
  char* ppos = strchr(line, '\n');
  if(ppos){
    *ppos = '\0';
  }
  ESP_LOGI(SD_CARD_TAG, "Read from file: '%s'", line);

  return ESP_OK;
}

esp_err_t init_sd_card(){
  esp_err_t r = ESP_OK;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };

  sdmmc_card_t* card;
  const char mount_point[] = MOUNT_POINT;
  ESP_LOGI(SD_CARD_TAG, "Initializing SD card");

  ESP_LOGI(SD_CARD_TAG, "Using SPI peripheral");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();

  spi_bus_config_t bus_cfg = {
    .mosi_io_num = PIN_NUM_MOSI,
    .miso_io_num = PIN_NUM_MISO,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4000,
  };

  r = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
  if(r != ESP_OK){
    ESP_LOGE(SD_CARD_TAG, "Failed to initialize SPI bus.");
    return ESP_FAIL;
  }

  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = PIN_NUM_CS;
  slot_config.host_id = host.slot;
  
  ESP_LOGI(SD_CARD_TAG, "Mounting file system");
  r = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
  
  if(r != ESP_OK){
    if(r == ESP_FAIL){
      ESP_LOGE(SD_CARD_TAG, "Failed to mount file system. "
          "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
    } 
    else{
      ESP_LOGE(SD_CARD_TAG, "Failed to initialize the card (%s). "
          "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(r));
    }
    return r;
  }
  ESP_LOGI(SD_CARD_TAG, "Filesystem mounted");

  sdmmc_card_print_info(stdout, card);

  return r;
}



