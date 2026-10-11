#include <cstring>
#include <errno.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "sd_card.h"

SD_CARD::SD_CARD(){
  esp_log_level_set(SD_CARD_TAG, ESP_LOG_INFO);
  ESP_LOGI(SD_CARD_TAG, "set SD_CARD_TAG log level: %d", ESP_LOG_ERROR);
}

esp_err_t SD_CARD::init(){
  esp_err_t r = ESP_OK;
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 10,
    .allocation_unit_size = 16 * 1024
  };

  sdmmc_card_t* pcard = nullptr;
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
  spi_bus_owned = r == ESP_OK;
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
      mounted_card = pcard;
      sdmmc_card_print_info(stdout, pcard);
    }
    else{
      ESP_LOGE(SD_CARD_TAG, "fail to moutn file system.");
    }
  }
  
  if(r != ESP_OK){ deinit(); }
  return r;
}

esp_err_t SD_CARD::write_data(const char* pfile_path, size_t file_path_size, char* data, char mode){
  std::lock_guard<std::mutex> guard(file_mutex);
  if(pfile_path == nullptr || data == nullptr || (mode != 'a' && mode != 'w' && mode != 'h')){
    return ESP_ERR_INVALID_ARG;
  }
  char write_file_path[100] {};
  const int length = snprintf(write_file_path, sizeof(write_file_path), "%s%s", MOUNT_POINT, pfile_path);
  if(length < 0 || static_cast<size_t>(length) >= sizeof(write_file_path)){
    return ESP_ERR_INVALID_SIZE;
  }
  FILE* pfile = nullptr;
  for(unsigned retry = 0; retry < 5 && pfile == nullptr; ++retry){
    pfile = fopen(write_file_path, mode == 'w' ? "w" : "a+");
    if(pfile == nullptr){ vTaskDelay(pdMS_TO_TICKS(50)); }
  }
  if(pfile == nullptr){
    ESP_LOGE(SD_CARD_TAG, "Failed to open %s: %s", write_file_path, strerror(errno));
    return ESP_FAIL;
  }
  esp_err_t r = ESP_OK;
  bool write = true;
  // Header mode preserves existing logs, including after a reboot.
  if(mode == 'h'){
    if(fseek(pfile, 0, SEEK_END) != 0){ r = ESP_FAIL; }
    else{
      const long size = ftell(pfile);
      if(size < 0){ r = ESP_FAIL; }
      write = (size == 0);
      ESP_LOGI(SD_CARD_TAG, "Sensor log: %ld existing bytes; %s", size, write ? "creating header" : "preserving existing log");
    }
  }
  if(r == ESP_OK && write && fputs(data, pfile) == EOF){ r = ESP_FAIL; }
  if(fflush(pfile) != 0){ r = ESP_FAIL; }
  if(fclose(pfile) != 0){ r = ESP_FAIL; }
  if(r == ESP_OK && write){ ESP_LOGI(SD_CARD_TAG, "Appended sensor log data"); }
  if(r != ESP_OK){ ESP_LOGE(SD_CARD_TAG, "Failed to write/close %s", write_file_path); }
  return r;
}

esp_err_t SD_CARD::read_data(const char* pfile_path){
  std::lock_guard<std::mutex> guard(file_mutex);
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

esp_err_t SD_CARD::open_sensor_log(FILE** file, long* size){
  if(file == nullptr || size == nullptr){ return ESP_ERR_INVALID_ARG; }
  std::lock_guard<std::mutex> guard(file_mutex);
  *file = fopen("/sdcard/sensor_log.csv", "rb");
  if(*file == nullptr){ return errno == ENOENT ? ESP_ERR_NOT_FOUND : ESP_FAIL; }
  if(fseek(*file, 0, SEEK_END) != 0 || (*size = ftell(*file)) < 0 ||
     fseek(*file, 0, SEEK_SET) != 0){
    fclose(*file);
    *file = nullptr;
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t SD_CARD::read_sensor_log(FILE* file, char* buffer, size_t size){
  if(file == nullptr || buffer == nullptr){ return ESP_ERR_INVALID_ARG; }
  std::lock_guard<std::mutex> guard(file_mutex);
  return fread(buffer, 1, size, file) == size ? ESP_OK : ESP_FAIL;
}

void SD_CARD::close_sensor_log(FILE* file){
  if(file == nullptr){ return; }
  std::lock_guard<std::mutex> guard(file_mutex);
  fclose(file);
}

void SD_CARD::deinit(){
  std::lock_guard<std::mutex> guard(file_mutex);
  if(mounted_card != nullptr){ esp_vfs_fat_sdcard_unmount(MOUNT_POINT, mounted_card); mounted_card = nullptr; }
  if(spi_bus_owned){ sdmmc_host_t host = SDSPI_HOST_DEFAULT(); spi_bus_free(static_cast<spi_host_device_t>(host.slot)); spi_bus_owned = false; }
}
