#include <cstring>
#include "driver/gpio.h"

#include "e_paper.h"

uint8_t EPAPER::transffer_buffer[DISPLAY_DISP_BYTES];

EPAPER::EPAPER(){
  esp_log_level_set(EPAPER_TAG, ESP_LOG_INFO);
  ESP_LOGI(EPAPER_TAG, "set EPAPER_TAG log level: %d", ESP_LOG_INFO);
  memset(transffer_buffer, 0x00, sizeof(transffer_buffer));
}

esp_err_t EPAPER::init_spi_bus(){
  esp_err_t r = ESP_OK;
  spi_bus_config_t bus_cfg = {
    .mosi_io_num = SPI_MOSI_PIN,
    .miso_io_num = -1,
    .sclk_io_num = SPI_SCK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = DISPLAY_DISP_BYTES + 1,
  }; 

  spi_device_interface_config_t spi_device_config = {
    .command_bits = 0,
    .address_bits = 0,
    .dummy_bits = 0,
    .mode = 0,
    .clock_speed_hz = SPI_CLOCK_SPEED,
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 8,
  };
  
  if(r == ESP_OK){
    r = spi_bus_initialize(EPAPER_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to spi bus initialize.");
    }
  }
  if(r == ESP_OK){
    r = spi_bus_add_device(EPAPER_SPI_HOST, &spi_device_config, &spi_handle);
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to add device to spi bus.");
    } 
  }
  return r;
}

esp_err_t EPAPER::init_gpio(){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){
    r = dc_pin.init(EPAPER_DC_PIN); 
    r |= rst_pin.init(EPAPER_RST_PIN, true); // set active low 
    r |= busy_pin.init(EPAPER_BUSY_PIN, true); // set active low 
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to initialize gpio.");
    }
  }
  if(r == ESP_OK){
    r = dc_pin.on();
    r |= rst_pin.on();
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to gpio level setting.");
    }
  }
  return r;
}

esp_err_t EPAPER::init_epaper(){
  esp_err_t r = ESP_OK;
  
  ESP_LOGI(EPAPER_TAG, "start to initialize e-paper.");

  if(r == ESP_OK){
    r = init_gpio();
  }  
  if(r == ESP_OK){
    r = init_spi_bus();
  }
  if(r == ESP_OK){
    r = execute_hw_reset();
  }
  if(r == ESP_OK){
    uint8_t send_data_buffer[3] = {0x17, 0x17, 0x17};
    r = send_command(BOOSTER_SOFT_START_COMMAND, send_data_buffer, sizeof(send_data_buffer));
    if(r != ESP_OK){
     ESP_LOGE(EPAPER_TAG, "fail to send BOOSTER_SOFT_START_COMMAND.");
    }
  }
  if(r == ESP_OK){
    r = send_command(POWER_ON_COMMAND, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(500)); 
    if(r != ESP_OK){
     ESP_LOGE(EPAPER_TAG, "fail to send POWER_ON_COMMAND.");
    }
  } 
  if(r == ESP_OK){
    r = wait_until_ready(); 
  }
  if(r == ESP_OK){
    r = send_command(PANEL_SETTING_COMMAND, PANEL_SETTINGS, sizeof(PANEL_SETTINGS));
    vTaskDelay(pdMS_TO_TICKS(5)); 
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to send PANEL_SETTING_COMMAND.");
    }
  }
  if(r == ESP_OK){
    r = send_command(SET_DISPLAY_RESOLUTION_COMMAND, DISPLAY_RESOLUTION_SETTINGS, sizeof(DISPLAY_RESOLUTION_SETTINGS));
    vTaskDelay(pdMS_TO_TICKS(5)); 
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to send SET_DISPLAY_RESOLUTION_COMMAND.");
    }
  }
  if(r == ESP_OK){
    r = send_command(STARTING_DATA_TRANSMISSION_COMMAND, 
        STARTING_DATA_TRANSMISSION_SETTINGS, sizeof(STARTING_DATA_TRANSMISSION_SETTINGS));
    vTaskDelay(pdMS_TO_TICKS(5));
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to send STARTING_DATA_TRANSMISSION_COMMAND.");
    }
  }
  if(r == ESP_OK){
    while(is_busy()){
      vTaskDelay(pdMS_TO_TICKS(5));
    }
    ESP_LOGI(EPAPER_TAG, "initialization completed successfully.");
  } 
  if(r != ESP_OK){
    ESP_LOGE(EPAPER_TAG, "fail to initialization.");
  }
  return r;
}

esp_err_t EPAPER::send_command(const uint8_t addr, const uint8_t* pdata_buffer, size_t buffer_size){
  esp_err_t r = ESP_OK;
  static spi_transaction_t spi_transaction;
  memset(&spi_transaction, 0, sizeof(spi_transaction));

  spi_transaction.flags = SPI_TRANS_USE_TXDATA,
  spi_transaction.length = 8;
  spi_transaction.tx_data[0] = addr;

  if(r == ESP_OK){
    r = dc_pin.off();
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to set dc_pin low.");
    }
  }
  if(r == ESP_OK){
    r = spi_device_transmit(spi_handle, &spi_transaction);
    ESP_LOGI(EPAPER_TAG, "send data: 0x%x", addr);
    if(r != ESP_OK){ 
      ESP_LOGE(EPAPER_TAG, "fail to send addr. Error code:%s", esp_err_to_name(r));
    }
  }
  if(r == ESP_OK){
    r = dc_pin.on();
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to set dc_pin hight.");
    }
  }
  
  if(r == ESP_OK){
    for(uint16_t i = 0; i < buffer_size; i++){
      spi_transaction.tx_data[0] = pdata_buffer[i];
      ESP_LOGI(EPAPER_TAG, "send data: 0x%x", pdata_buffer[i]);
      if(r == ESP_OK){ 
        r = spi_device_polling_transmit(spi_handle, &spi_transaction);
        if(r != ESP_OK){
          ESP_LOGE(EPAPER_TAG, "fail to send SPI transmit. Error code:%s", esp_err_to_name(r));
        }
      }
    }
  }
  ESP_LOGI(EPAPER_TAG, "successfully sent command.");
  return r;
}

esp_err_t EPAPER::send_frame(const uint8_t* pdata_buffer, size_t buffer_size){
  esp_err_t r = ESP_OK;
  static spi_transaction_t spi_transaction = {
    .flags = SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL,
    .length = DISPLAY_DISP_BYTES * 8,
    .user = (void*) 0,
  };
  spi_transaction.tx_buffer = pdata_buffer;
  if(r == ESP_OK){
    r = spi_device_polling_transmit(spi_handle, &spi_transaction);
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to send transmit frame. Error code:%s", esp_err_to_name(r));
    }
  }
  return r;
}

uint8_t EPAPER::is_busy(){
  return busy_pin.read();
}

esp_err_t EPAPER::wait_until_ready(){
  esp_err_t r = ESP_OK;
  while(is_busy()){
    ESP_LOGI(EPAPER_TAG, "wait until e-paper is ready");
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  return r;
}

esp_err_t EPAPER::init(){
  esp_err_t r = ESP_OK;
  
  if(r == ESP_OK){
    r = init_epaper();
  }

  return r;
}

esp_err_t EPAPER::execute_hw_reset(){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){
    r = rst_pin.off();
    vTaskDelay(pdMS_TO_TICKS(10));
    r |= rst_pin.on();
    vTaskDelay(pdMS_TO_TICKS(1));
    r |= rst_pin.off();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  return r;
}

esp_err_t EPAPER::turn_on_display(){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){
    r = send_command(DISPLAY_REFRESH_COMMAND, NULL, 0);
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to send DISPLAY_REFRESH_COMMAND");
    }
  }
  if(r == ESP_OK){
    while(is_busy()){
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }

  return r;
}

esp_err_t EPAPER::display(const uint8_t* pblack_image, size_t black_image_size,
    const uint8_t* pred_image, size_t red_image_size){
  esp_err_t r = ESP_OK;
  
  static spi_transaction_t spi_transaction = {
    .flags = SPI_TRANS_USE_TXDATA,
    .length = 8,
  };
  if(r == ESP_OK){
    while(is_busy()){
      vTaskDelay(pdMS_TO_TICKS(5));
      ESP_LOGI(EPAPER_TAG, "waiting for epaper is ready.");
    }
  }
  if(r == ESP_OK){
    r = send_command(DISPLAY_START_TRANSMISSION_1, NULL, 0);
  }
  if(r == ESP_OK){
    r = send_frame(pblack_image, black_image_size);
  }
  if(r == ESP_OK){
    r = send_command(DISPLAY_START_TRANSMISSION_2, NULL, 0);
  }
  if(r == ESP_OK){
    r = send_frame(pred_image, red_image_size);
  }
  if(r == ESP_OK){
    r = turn_on_display();
  }

  return r;
}

esp_err_t EPAPER::clear_screen(){
  esp_err_t r = ESP_OK;
  static spi_transaction_t spi_transaction = {
    .flags = SPI_TRANS_USE_TXDATA,
    .length = 8,
  };

  spi_transaction.tx_data[0] = DISPLAY_START_TRANSMISSION_1;

  if(r == ESP_OK){
    r = dc_pin.off();
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to set dc_pin low.");
    }
  }
  if(r == ESP_OK){
    r = spi_device_transmit(spi_handle, &spi_transaction);
    if(r != ESP_OK){ 
      ESP_LOGE(EPAPER_TAG, "fail to send addr. Error code:%s", esp_err_to_name(r));
    }
  }
  if(r == ESP_OK){
    r = dc_pin.on();
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to set dc_pin hight.");
    }
  }
  if(r == ESP_OK){
    memset(transffer_buffer, 0x00, sizeof(transffer_buffer));
    r = send_frame(transffer_buffer, sizeof(transffer_buffer));
  }
  
  spi_transaction.tx_data[0] = DISPLAY_START_TRANSMISSION_2;

  if(r == ESP_OK){
    r = dc_pin.off();
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to set dc_pin low.");
    }
  }
  if(r == ESP_OK){
    r = spi_device_transmit(spi_handle, &spi_transaction);
    if(r != ESP_OK){ 
      ESP_LOGE(EPAPER_TAG, "fail to send addr. Error code:%s", esp_err_to_name(r));
    }
  }
  if(r == ESP_OK){
    r = dc_pin.on();
    if(r != ESP_OK){
      ESP_LOGE(EPAPER_TAG, "fail to set dc_pin hight.");
    }
  }
  
  if(r == ESP_OK){
    r = send_frame(transffer_buffer, sizeof(transffer_buffer));
  }
  if(r == ESP_OK){
    r = turn_on_display();
  }
  return r;
}

esp_err_t EPAPER::display_black(){
  esp_err_t r = ESP_OK;
  
  static spi_transaction_t spi_transaction = {
    .flags = SPI_TRANS_USE_TXDATA,
    .length = 8,
  };
  if(r == ESP_OK){
    while(is_busy()){
      vTaskDelay(pdMS_TO_TICKS(5));
      ESP_LOGI(EPAPER_TAG, "waiting for epaper is ready.");
    }
  }
  if(r == ESP_OK){
    r = send_command(DISPLAY_START_TRANSMISSION_1, NULL, 0);
  }
  if(r == ESP_OK){
    memset(transffer_buffer, 0xFF, sizeof(transffer_buffer));
    r = send_frame(transffer_buffer, sizeof(transffer_buffer));
    memset(transffer_buffer, 0x00, sizeof(transffer_buffer)); 
  }
  if(r == ESP_OK){
    r = turn_on_display();
  }

  return r;
}
