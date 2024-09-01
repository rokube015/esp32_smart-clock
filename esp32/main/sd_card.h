#ifndef SD_CARD_H
#define SD_CARD_H
#include <esp_log.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define MAX_SDCARD_LINE_CHAR_SIZE 128
#define MOUNT_POINT "/sdcard"

/*SPI Port for SD Card access*/
#define PIN_NUM_MISO  GPIO_NUM_19
#define PIN_NUM_MOSI  GPIO_NUM_23
#define PIN_NUM_CLK   GPIO_NUM_18
#define PIN_NUM_CS    GPIO_NUM_5


esp_err_t write_sd_card_file(const char* path, char* data, char mode);

esp_err_t read_sd_card_file(const char *path);

esp_err_t init_sd_card();
#endif
