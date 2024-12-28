#include <cstring>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "sd_card.h"

SD_CARD::SD_CARD(){
  esp_log_level_set(SD_CARD_TAG, ESP_LOG_INFO);
  ESP_LOGI(SD_CARD_TAG, "set SD_CARD_TAG log level: %d", ESP_LOG_INFO);
}

esp_err_t SD_CARD::init(){
  esp_err_t r = ESP_OK;
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };

  sdmmc_card_t* pcard;
  ESP_LOGI(SD_CARD_TAG, "Initializing SD card");
  ESP_LOGI(SD_CARD_TAG, "Using SPI peripheral");

  //initialize spi bus 
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();

  spi_bus_config_t bus_cfg = {
    .mosi_io_num = SPI_MOSI_PIN,
    .miso_io_num = SPI_MISO_PIN,
    .sclk_io_num = SPI_CLK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4000,
  };

  r = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
  if(r != ESP_OK){
    ESP_LOGE(SD_CARD_TAG, "failed to initialize SPI bus.");
    r = ESP_FAIL;
  }

  //setup sdcard
  if(r == ESP_OK){
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SPI_CS_PIN;
    slot_config.host_id = (spi_host_device_t)host.slot;

    ESP_LOGI(SD_CARD_TAG, "Mounting file system");
    
    r = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &pcard);
    if(r == ESP_OK){
      ESP_LOGI(SD_CARD_TAG, "mount filesystem.");
      sdmmc_card_print_info(stdout, pcard);
    }
    else{
      ESP_LOGE(SD_CARD_TAG, "fail to moutn file system.");
    }
  }
  
  return r;
}

esp_err_t SD_CARD::write_data(const char* pfile_path, size_t file_path_size, char* data, char mode){
  esp_err_t r = ESP_OK;
  ESP_LOGI(SD_CARD_TAG, "opening sd_card file: %s", pfile_path);
  char write_file_path[50];
  FILE* pfile = NULL;
  
  if((sizeof(MOUNT_POINT) + file_path_size) < sizeof(write_file_path)){
    strncat(write_file_path, MOUNT_POINT, sizeof(write_file_path)-strlen(write_file_path)-1);
    strncat(write_file_path, pfile_path, sizeof(write_file_path)-strlen(write_file_path)-1);
  }
  else{
    r  = ESP_FAIL;
    ESP_LOGW(SD_CARD_TAG, "file path is too large.");
  }
  
  switch(mode){
    case 'a': 
      pfile = fopen(write_file_path, "a");
      break;
    case 'w':
      pfile = fopen(write_file_path, "w");
      break;
    default:
      ESP_LOGW(SD_CARD_TAG, "invalid write mode: %c", mode);
      r = ESP_FAIL;
  }
  if((r != ESP_OK) || (pfile == NULL)){
    ESP_LOGE(SD_CARD_TAG, "fail to open file for writing.");
    r = ESP_FAIL;
  }
  if(r == ESP_OK){
    fprintf(pfile, data);
    fclose(pfile);
    ESP_LOGI(SD_CARD_TAG, "file to write data to sd card.");
  }
  
  return r;
}

esp_err_t SD_CARD::read_data(const char* pfile_path){
  esp_err_t r = ESP_OK;
  ESP_LOGI(SD_CARD_TAG, "read file path: %s", pfile_path);

  FILE* pfile = fopen(pfile_path, "r");
  if(pfile == NULL){
    ESP_LOGE(SD_CARD_TAG, "fail to open file for reading.");
    r = ESP_FAIL;
  }
  
  if(r == ESP_OK){
    char line[MAX_SDCARD_LINE_CHAR_SIZE];
    fgets(line, sizeof(line), pfile);
    fclose(pfile);

    char* ppos = strchr(line, '\n');
    if(ppos){
      *ppos = '\0';
    }
    ESP_LOGI(SD_CARD_TAG, "read from file: '%s'", line);
  }

  return r;
}
