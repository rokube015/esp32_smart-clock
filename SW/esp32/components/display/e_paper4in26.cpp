#include <cstring>
#include "driver/gpio.h"

#include "e_paper.h"

uint8_t EPAPER4IN26::transffer_buffer[DISPLAY_DISP_BYTES];
EPAPER4IN26::state_e EPAPER4IN26::mstate;

EPAPER4IN26::EPAPER4IN26(){
  esp_log_level_set(EPAPER_TAG, ESP_LOG_INFO);
  ESP_LOGI(EPAPER_TAG, "set EPAPER_TAG log level: %d", ESP_LOG_INFO);
  memset(transffer_buffer, 0xFF, sizeof(transffer_buffer));
  mstate = state_e::NOT_INITIALIZED;
}

esp_err_t EPAPER4IN26::init_spi_bus(){
  esp_err_t r = ESP_OK;
  spi_bus_config_t bus_cfg = {
    .mosi_io_num = SPI_MOSI_PIN,
    .miso_io_num = -1,
    .sclk_io_num = SPI_SCK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = MAX_SPI_TARANSFER_SIZE + 1,
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

esp_err_t EPAPER4IN26::init_gpio(){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){
    r = dc_pin.init(EPAPER_DC_PIN); 
    r |= rst_pin.init(EPAPER_RST_PIN, true); // set active low 
    r |= busy_pin.init(EPAPER_BUSY_PIN); 
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

esp_err_t EPAPER4IN26::init_epaper(){
  esp_err_t r = ESP_OK;
  
  ESP_LOGI(EPAPER_TAG, "start to initialize e-paper.");
  
  if(r == ESP_OK){
    r = execute_hw_reset();
  }
  if(r == ESP_OK){
    r = wait_until_ready(); 
  }
  if(r == ESP_OK){
    r = send_command(SW_RESET_COMMAND, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
  } 
  if(r == ESP_OK){
    r = wait_until_ready(); 
  }
  if(r == ESP_OK){
    r = send_command(TEMPERATURE_SENSOR_CONTROL_COMMAND, 
                     &USE_INTERNAL_TEMPERATURE_SENSOR, sizeof(USE_INTERNAL_TEMPERATURE_SENSOR));
  }
  if(r == ESP_OK){
    r = send_command(BOOSTER_SOFT_START_CONTROL_COMMAND, SOFT_START_CONTROL_SETTING, 
                     sizeof(SOFT_START_CONTROL_SETTING));
  }
  if(r == ESP_OK){
    r = send_command(DRIVER_OUTPUT_CONTROL_COMMAND, DRIVER_OUTPUT_CONTROL_SETTING, 
                     sizeof(DRIVER_OUTPUT_CONTROL_SETTING));
  }
  if(r == ESP_OK){
    r = send_command(BORDER_WAVEFORM_CONTROL_COMMAND, &BORDER_WAVEFORM_CONTROL_SETTING,
                     sizeof(BORDER_WAVEFORM_CONTROL_SETTING));
  }
  if(r == ESP_OK){
    r = send_command(DATA_ENTRY_MODE_COMMAND, &DATA_ENTRY_MODE_SETTING,
                     sizeof(DATA_ENTRY_MODE_SETTING));
  }
  if(r == ESP_OK){
    r = set_windows(0, DISPLAY_RESOLUTION_HEIGHT - 1, DISPLAY_RESOLUTION_WIDTH - 1, 0); 
  }
  if(r == ESP_OK){
    r = set_cursur(0, 0);
  }
  if(r == ESP_OK){
    r = wait_until_ready(); 
    ESP_LOGI(EPAPER_TAG, "initialization completed successfully.");
  } 
  if(r != ESP_OK){
    ESP_LOGE(EPAPER_TAG, "fail to initialization.");
  }
  return r;
}

esp_err_t EPAPER4IN26::send_command(const uint8_t addr, const uint8_t* pdata_buffer, size_t buffer_size){
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

esp_err_t EPAPER4IN26::send_frame(const uint8_t* pdata_buffer, size_t buffer_size){
  esp_err_t r = ESP_OK;
  size_t offset = 0;
  static spi_transaction_t spi_transaction = {
    .flags = 0,
    .user = (void*) 0,
  };
  if(r == ESP_OK){
    r = spi_device_acquire_bus(spi_handle, portMAX_DELAY);
  }
  if(r == ESP_OK){
    while(buffer_size > 0){
      size_t transfer_size = (buffer_size > MAX_SPI_TARANSFER_SIZE)? MAX_SPI_TARANSFER_SIZE : buffer_size;
      spi_transaction.tx_buffer = pdata_buffer + offset;
      spi_transaction.length = transfer_size * 8;
      spi_transaction.flags = (buffer_size > transfer_size) ? SPI_TRANS_CS_KEEP_ACTIVE : 0;
      r = spi_device_transmit(spi_handle, &spi_transaction);
      ESP_LOGI(EPAPER_TAG, "send to frame"); 
      if(r != ESP_OK){
        ESP_LOGE(EPAPER_TAG, "fail to send transmit frame. Error code:%s", esp_err_to_name(r));
        break; 
      }
      offset += transfer_size;
      buffer_size -= transfer_size;
    }
    spi_device_release_bus(spi_handle);
  }

  return r;
}

uint8_t EPAPER4IN26::is_busy(){
  return busy_pin.read();
}

esp_err_t EPAPER4IN26::wait_until_ready(){
  esp_err_t r = ESP_OK;
  while(is_busy()){
    ESP_LOGI(EPAPER_TAG, "wait until e-paper is ready");
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  return r;
}

esp_err_t EPAPER4IN26::set_windows(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end){
  esp_err_t r = ESP_OK;
  
  uint8_t send_data[4];

  if(r == ESP_OK){
    send_data[0] = x_start & 0xFF;
    send_data[1] = (x_start >> 8)&0x03;
    send_data[2] = x_end & 0xFF;
    send_data[3] = (x_end >> 8)&0x03;
    r = send_command(SET_X_START_END_POSITION_COMMAND, send_data, sizeof(send_data));
  }
  if(r == ESP_OK){
    send_data[0] = y_start & 0xFF;
    send_data[1] = (y_start >> 8)&0x03;
    send_data[2] = y_end & 0xFF;
    send_data[3] = (y_end >> 8)&0x03;
    r = send_command(SET_Y_START_END_POSITION_COMMAND, send_data, sizeof(send_data));
  }
  return r;
}

esp_err_t EPAPER4IN26::init(){
  esp_err_t r = ESP_OK;
  
  if(r == ESP_OK){
    r = init_gpio();
  }  
  if(r == ESP_OK){
    r = init_spi_bus();
  }
  if(r == ESP_OK){
    r = init_epaper();
  }
  if(r == ESP_OK){
    mstate = state_e::INITIALIZED;
  }

  return r;
}

esp_err_t EPAPER4IN26::execute_hw_reset(){
  esp_err_t r = ESP_OK;
  if(r == ESP_OK){
    ESP_LOGI(EPAPER_TAG, "execute hw reset.");
    r = rst_pin.off();
    vTaskDelay(pdMS_TO_TICKS(20));
    r |= rst_pin.on();
    vTaskDelay(pdMS_TO_TICKS(1));
    r |= rst_pin.off();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return r;
}

esp_err_t EPAPER4IN26::turn_on_display(){
  esp_err_t r = ESP_OK;
  uint8_t send_data = 0;
  if(r == ESP_OK){
    send_data = 0xF7;
    r = send_command(DISPLAY_UPDATE_CONTROL_2_COMMAND, &send_data, sizeof(send_data));
  }
  if(r == ESP_OK){
    r = send_command(MASTER_ACTIVATION_COMMAND, NULL, 0);
  }
  if(r == ESP_OK){
    while(is_busy()){
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
  return r;
}

esp_err_t EPAPER4IN26::display(const uint8_t* pblack_image, size_t black_image_size){
  esp_err_t r = ESP_OK;
  
  if(r == ESP_OK){
    r = wait_until_ready();
  }
  if(r == ESP_OK){
    r = send_command(WRITE_RAM_0x24_COMMAND, NULL, 0);
  }
  if(r == ESP_OK){
    r = send_frame(pblack_image, black_image_size);
  }
  if(r == ESP_OK){
    r = turn_on_display();
  }
  if(r == ESP_OK){
    mstate = state_e::RUNNING;
  }
  return r;
}

esp_err_t EPAPER4IN26::clear_screen(){
  esp_err_t r = ESP_OK;
  
  if(r == ESP_OK){
    r = wait_until_ready();
  }
  if(r == ESP_OK){
    r = send_command(WRITE_RAM_0x24_COMMAND, NULL, 0);
  } 
  if(r == ESP_OK){
    memset(transffer_buffer, 0xFF, sizeof(transffer_buffer));
    r = send_frame(transffer_buffer, sizeof(transffer_buffer));
  }
  if(r == ESP_OK){
    r = send_command(WRITE_RAM_0x26_COMMAND, NULL, 0);
  }
  if(r == ESP_OK){
    r = send_frame(transffer_buffer, sizeof(transffer_buffer));
  }
  if(r == ESP_OK){
    r = turn_on_display();
  }
  return r;
}

esp_err_t EPAPER4IN26::set_sleep_mode(){
  esp_err_t r = ESP_OK;
  
  if(r == ESP_OK){
    uint8_t send_data = 0x03; //enter deep sleep mode
    r = send_command(DEEP_SLEEP_MODE_COMMAND, &send_data, sizeof(send_data));
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  if(r == ESP_OK){
    mstate = state_e::SLEEP;
    ESP_LOGI(EPAPER_TAG, "set sleep mode.");
  }
  return r;
}

esp_err_t EPAPER4IN26::set_cursur(uint16_t x_position, uint16_t y_position){
  esp_err_t r = ESP_OK;
  uint8_t send_data[2];
  if(r == ESP_OK){
    send_data[0] = x_position & 0xFF;
    send_data[1] = (x_position >> 8) & 0x03;
    r = send_command(SET_X_ADDRESS_COUNTER_COMMAND, send_data, sizeof(send_data));
  }
  if(r == ESP_OK){
    send_data[0] = y_position & 0xFF;
    send_data[1] = (y_position >> 8) & 0x03;
    r = send_command(SET_Y_ADDRESS_COUNTER_COMMAND, send_data, sizeof(send_data));
  }
  return r;
}
