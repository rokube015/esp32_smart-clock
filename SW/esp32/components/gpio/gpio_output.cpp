#include "gpio_interface.h"

namespace GpioInterface{
  esp_err_t GpioOutput::minit(const gpio_num_t pin, const bool active_low){
    esp_err_t status = ESP_OK;

    mactive_low = active_low;
    mpin = pin;

    gpio_config_t cfg;
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;

    status |= gpio_config(&cfg);

    return status;
  }

  GpioOutput::GpioOutput(const gpio_num_t pin, const bool active_low){
    minit(pin, active_low);
  }

  GpioOutput::GpioOutput(const gpio_num_t pin){
    minit(pin, false);
  }

  GpioOutput::GpioOutput(void){        

  }

  esp_err_t GpioOutput::init(const gpio_num_t pin, const bool active_low){
    return minit(pin, active_low);
  }

  esp_err_t GpioOutput::init(const gpio_num_t pin){
    return minit(pin, false);
  }

  esp_err_t GpioOutput::on(void){
    mlevel = true;
    int gpio_level = (mactive_low ? 0 : 1);
    ESP_LOGD(GPIO_TAG, "set gpio %d pin: %d, active_low: %d", 
        mpin, gpio_level, mactive_low);
    return gpio_set_level(mpin, gpio_level);
  }

  esp_err_t GpioOutput::off(void){
    mlevel = false;
    int gpio_level = (mactive_low ? 1 : 0);
    ESP_LOGD(GPIO_TAG, "set gpio %d pin: %d, active_low: %d", 
        mpin, gpio_level, mactive_low);
    return gpio_set_level(mpin, gpio_level);
  }

  esp_err_t GpioOutput::toggle(void){
    mlevel = mlevel ? 0 : 1;
    return gpio_set_level(mpin, mlevel ? 1 : 0);
  }

  esp_err_t GpioOutput::set_level(int level){
    mlevel = mactive_low ? !level : level;
    return gpio_set_level(mpin, level);
  }
}
