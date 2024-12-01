#include "gpio_interface.h"

namespace GpioInterface{ 
  bool GpioInput::minterrupt_service_installed{false};
  portMUX_TYPE GpioInput::mevent_change_mutex = portMUX_INITIALIZER_UNLOCKED;

  ESP_EVENT_DEFINE_BASE(INPUT_EVENTS);

  void IRAM_ATTR GpioInput::gpio_isr_callback(void *args){
    int32_t pin = (reinterpret_cast<interrupt_args_t*>(args))->mpin;
    bool custom_event_handler_set = (reinterpret_cast<interrupt_args_t*>(args))->mcustom_event_handler_set;
    bool event_handler_set = 
      (reinterpret_cast<interrupt_args_t*>(args))->mevent_handler_set;
    bool queue_enabled = 
      (reinterpret_cast<interrupt_args_t*>(args))->mqueue_enable;
    esp_event_loop_handle_t custom_event_loop_handle = 
      (reinterpret_cast<interrupt_args_t*>(args))->mcustom_event_loop_handle;
    QueueHandle_t queue_handle = 
      (reinterpret_cast<interrupt_args_t*>(args))->mqueue_handle;

    if(queue_enabled){
      xQueueSendFromISR(queue_handle, &pin, NULL);
    }
    else if(custom_event_handler_set){
      esp_event_isr_post_to(custom_event_loop_handle, INPUT_EVENTS, pin, nullptr, 0, nullptr);
    }
    else if(event_handler_set){
      esp_event_isr_post(INPUT_EVENTS, pin, nullptr, 0, nullptr);
    }

    esp_event_isr_post(INPUT_EVENTS, pin, nullptr, 0, nullptr);
  }

  esp_err_t GpioInput::minit(const gpio_num_t pin, const bool activeLow){
    esp_err_t r = ESP_OK;

    mactive_low = activeLow;
    minterrupt_args.mpin = pin;

    gpio_config_t cfg;
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;

    r = gpio_config(&cfg);

    return r;
  }

  GpioInput::GpioInput(const gpio_num_t pin, const bool active_low){
    minit(pin, active_low);
  }

  GpioInput::GpioInput(const gpio_num_t pin){
    minit(pin, false);
  }

  GpioInput::GpioInput(void){

  }

  esp_err_t GpioInput::init(const gpio_num_t pin, const bool active_low){
    return minit(pin, active_low);
  }

  esp_err_t GpioInput::init(const gpio_num_t pin){
    return minit(pin, false);
  }

  int GpioInput::read(void){
    return mactive_low ? !gpio_get_level(minterrupt_args.mpin) : gpio_get_level(minterrupt_args.mpin);
  }

  esp_err_t GpioInput::enable_pull_up(void){
    return gpio_set_pull_mode(minterrupt_args.mpin, GPIO_PULLUP_ONLY);
  }

  esp_err_t GpioInput::disable_pull_up(void){
    return gpio_set_pull_mode(minterrupt_args.mpin, GPIO_FLOATING);
  }

  esp_err_t GpioInput::enable_pull_down(void){
    return gpio_set_pull_mode(minterrupt_args.mpin, GPIO_PULLDOWN_ONLY);
  }

  esp_err_t GpioInput::disable_pull_down(void){
    return gpio_set_pull_mode(minterrupt_args.mpin, GPIO_FLOATING);
  }

  esp_err_t GpioInput::enable_pull_up_and_pull_down(void){
    return gpio_set_pull_mode(minterrupt_args.mpin, GPIO_PULLUP_PULLDOWN);
  }

  esp_err_t GpioInput::disable_pull_up_and_pull_down(void){
    return gpio_set_pull_mode(minterrupt_args.mpin, GPIO_FLOATING);
  }

  esp_err_t GpioInput::enable_interrupt(gpio_int_type_t int_type){
    esp_err_t r = ESP_OK;

    // Invert triggers if active low is enabled
    if(mactive_low){
      switch(int_type){
        case GPIO_INTR_POSEDGE:
          int_type = GPIO_INTR_NEGEDGE;
          break;
        case GPIO_INTR_NEGEDGE:
          int_type = GPIO_INTR_POSEDGE;
          break;
        case GPIO_INTR_LOW_LEVEL:
          int_type = GPIO_INTR_HIGH_LEVEL;
          break;
        case GPIO_INTR_HIGH_LEVEL:
          int_type = GPIO_INTR_LOW_LEVEL;
          break;
        default:
          break;
      }
    }

    if(!minterrupt_service_installed){
      r = gpio_install_isr_service(0);
      if(r == ESP_OK){
        minterrupt_service_installed = true;
      }
    }

    if(r == ESP_OK){
      r = gpio_set_intr_type(minterrupt_args.mpin, int_type);
    }

    if(r == ESP_OK){
      r = gpio_isr_handler_add(minterrupt_args.mpin, gpio_isr_callback, (void *)&minterrupt_args);
    }
    return r;
  }

  esp_err_t GpioInput::set_event_handler(esp_event_handler_t Gpio_e_h){
    esp_err_t r = ESP_OK;
    
    taskENTER_CRITICAL(&mevent_change_mutex);
    
    r = mclear_event_handlers();
    
    r |= esp_event_handler_instance_register(INPUT_EVENTS, minterrupt_args.mpin, Gpio_e_h, 0, nullptr);

    if(r == ESP_OK){
      mevent_handler_set = true;
    }

    taskENTER_CRITICAL(&mevent_change_mutex);

    return r;
  }

  esp_err_t GpioInput::set_event_handler(esp_event_loop_handle_t Gpio_e_l, esp_event_handler_t Gpio_e_h){
    esp_err_t r = ESP_OK;
    taskENTER_CRITICAL(&mevent_change_mutex);

    r = mclear_event_handlers();
    r |= esp_event_handler_instance_register_with(Gpio_e_l, INPUT_EVENTS, minterrupt_args.mpin, Gpio_e_h, 0, nullptr);

    if(r == ESP_OK){
      mevent_handle = Gpio_e_h;
      minterrupt_args.mcustom_event_loop_handle = Gpio_e_l;
      minterrupt_args.mcustom_event_handler_set = true;
    }

    taskEXIT_CRITICAL(&mevent_change_mutex);

    return r;
  }

  void GpioInput::set_queue_handle(QueueHandle_t Gpio_e_q){
    taskENTER_CRITICAL(&mevent_change_mutex);
    mclear_event_handlers();
    minterrupt_args.mqueue_handle = Gpio_e_q;
    minterrupt_args.mqueue_enable = true;
    taskEXIT_CRITICAL(&mevent_change_mutex);
  }

  esp_err_t GpioInput::mclear_event_handlers(){
    esp_err_t r = ESP_OK;

    if(minterrupt_args.mcustom_event_handler_set){
      esp_event_handler_unregister_with(minterrupt_args.mcustom_event_loop_handle,
          INPUT_EVENTS, minterrupt_args.mpin, mevent_handle);
      minterrupt_args.mcustom_event_handler_set = false;
    }
    else if(minterrupt_args.mevent_handler_set){
      esp_event_handler_instance_unregister(INPUT_EVENTS, minterrupt_args.mpin, nullptr);
      minterrupt_args.mevent_handler_set = false;
    }

    minterrupt_args.mqueue_handle = nullptr;
    minterrupt_args.mqueue_enable = false;

    return r;
  }
} // Namespace GpioInterface
