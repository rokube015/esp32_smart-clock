#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "scd40.h"
#include "sd_card.h"

static const char* MAIN_TAG = "app_main_tag";

void app_main(void){
  const char* pScd40_data_filepath = MOUNT_POINT"/scd40.txt";
  char sdcard_write_data[MAX_SDCARD_LINE_CHAR_SIZE] = "\0";
  
  esp_err_t r = ESP_OK;
  esp_log_level_set(MAIN_TAG, ESP_LOG_DEBUG);
  
  scd40_value_t scd40_value = {
    .co2 = 0,
    .temperature = 0.0, 
    .relative_humidity = 0.0
  };
  float temperature_offset = 0.0;

  r = init_scd40();
  if(r != ESP_OK){
   ESP_LOGE(MAIN_TAG, "Faild to set up scd40.");
  }

  if(r == ESP_OK){
    uint64_t serial_number = 0;
    r = get_scd40_serial_number(&serial_number);
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "Faild to read serial number");
    } 
    if(r == ESP_OK){
      ESP_LOGI(MAIN_TAG, "SCD40 Serial Number:%llu", serial_number);
      vTaskDelay(5/ portTICK_PERIOD_MS);
    }
  }
  
  if(r == ESP_OK){
    r = get_scd40_temperature_offset(&temperature_offset);
    if(r == ESP_OK){
      ESP_LOGI(MAIN_TAG, "default temperature_offset is %f", temperature_offset);
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    else if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "faild to read temperature offset");
    }
  }

  if(r == ESP_OK){
    temperature_offset = 0.0;
    r = set_scd40_temperature_offset(temperature_offset);
    vTaskDelay(5/ portTICK_PERIOD_MS);
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "Failed to set temperature offset.");
    }
    ESP_LOGI(MAIN_TAG, "set temperature offset: %f", temperature_offset);
  }
  if(r == ESP_OK){
    r =  init_sd_card();
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "Failed to initialize initilize SD Card setup.");
    }
  }
  
  if(r == ESP_OK){
    snprintf(sdcard_write_data, sizeof(sdcard_write_data), "CO2[rpm] \tTemperature[degree] \tHumidity[%%RH]\n");
    r = write_sd_card_file(pScd40_data_filepath, sdcard_write_data, 'w');
    if(r == ESP_OK){
      ESP_LOGI(MAIN_TAG, "Finish set up sd card.");
    }
    if(r != ESP_OK){
      ESP_LOGE(MAIN_TAG, "Failed to write data to sd card.");
    }
  }
  
  while(1){
    if(r == ESP_OK){
      r = start_scd40_periodic_measurement();
      if(r != ESP_OK){
        ESP_LOGE(MAIN_TAG, "Failed to send start periodic measurecommand to scd40.");
      }
    }
    if(r == ESP_OK){
      vTaskDelay(5000/ portTICK_PERIOD_MS);
      r = get_scd40_sensor_data(&scd40_value);
      if(r != ESP_OK){
        ESP_LOGE(MAIN_TAG, "Failed to read scd40 sensor data");
      }
    }
    if(r == ESP_OK){
      ESP_LOGI(MAIN_TAG, "co2:%d, temperature:%f, humidity:%f",
          scd40_value.co2, scd40_value.temperature, scd40_value.relative_humidity);
      vTaskDelay(5000/ portTICK_PERIOD_MS);
      r = stop_scd40_periodic_measurement();
      if(r != ESP_OK){
        ESP_LOGE(MAIN_TAG, "Failed to send stop periodic measure command to scd40.");
      }
    }
    if(r == ESP_OK){
      snprintf(sdcard_write_data, sizeof(sdcard_write_data),
          "%d\t%f\t%f\n",scd40_value.co2, scd40_value.temperature, scd40_value.relative_humidity);
      r = write_sd_card_file(pScd40_data_filepath, sdcard_write_data, 'a');
      ESP_LOGI(MAIN_TAG, "Write scd40 data to sd card.");
      if(r != ESP_OK){
        ESP_LOGE(MAIN_TAG, "Failed to write data to sd card.");
      }
      vTaskDelay(1000/ portTICK_PERIOD_MS);
    }
  }
}
