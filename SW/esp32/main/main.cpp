#include <iostream>

#include "esp_log.h"
#include "esp_check.h"
#include "i2c_base.h"
#include "bme280.h"

constexpr static int I2C_SDA = 21;
constexpr static int I2C_SCL = 22;
constexpr static uint32_t I2C_CLK_SPEED_HZ = 800000;
static const char* MAIN_TAG = "main";

i2c_base::I2C i2c {I2C_NUM_0};

extern "C" void app_main(void){ 
  esp_log_level_set(MAIN_TAG, ESP_LOG_DEBUG); 
  esp_err_t r = ESP_OK;  
  ESP_LOGI(MAIN_TAG, "Start main"); 
  
  BME280 Bme280;

  // Initialise the I2C
  if(r == ESP_OK){ 
    r = i2c.init_master(I2C_SDA, I2C_SCL, I2C_CLK_SPEED_HZ, true, true);
  }
  
  // Initialize the BME280I2C device
  if(r == ESP_OK){ 
    Bme280.init_i2c(&i2c, 0x76);
  }
  if(r == ESP_OK){ 
    r = Bme280.init();
  }
  if(r == ESP_OK){ 
    Bme280.set_config_filter(1);
  }
  
  float bme280_temperature{};
  float bme280_pressure{};
  double bme280_humidity{};
  int bme280_device_id{};

  while(true){
    bme280_device_id = Bme280.get_deviceID();
    Bme280.get_all_results(&bme280_temperature, &bme280_humidity, &bme280_pressure);
    std::cout << "==================================================\n";
    std::cout << "BME280 Temperature: " << bme280_temperature << "c\n";
    std::cout << "BME280 Humidity   : " << bme280_humidity << "%\n";
    std::cout << "BME280 Pressure   : " << bme280_pressure << "Pa\n";
    std::cout << "BME280 Device ID  : " << bme280_device_id << '\n';
    std::cout << "==================================================\n";

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
