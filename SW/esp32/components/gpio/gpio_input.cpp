#include "gpio_interface.h"

namespace GpioInterface{ 
  bool GpioInput::minterrupt_service_installed{false};
  portMUX_TYPE GpioInput::mevent_change_mutex = portMUX_INITIALIZER_UNLOCKED;

  ESP_EVENT_DEFINE_BASE(INPUT_EVENTS);

  void IRAM_ATTR GpioInput::gpio_isr_callback(void *args){
    taskENTER_CRITICAL_ISR(&mevent_change_mutex);
    const interrupt_args_t state = *static_cast<interrupt_args_t*>(args);
    taskEXIT_CRITICAL_ISR(&mevent_change_mutex);
    const int32_t pin = state.mpin;
    const bool custom_event_handler_set = state.mcustom_event_handler_set;
    const bool event_handler_set = state.mevent_handler_set;
    const bool queue_enabled = state.mqueue_enable;
    const esp_event_loop_handle_t custom_event_loop_handle = state.mcustom_event_loop_handle;
    const QueueHandle_t queue_handle = state.mqueue_handle;

    if(queue_enabled){
      xQueueSendFromISR(queue_handle, &pin, NULL);
    }
    else if(custom_event_handler_set){
      esp_event_isr_post_to(custom_event_loop_handle, INPUT_EVENTS, pin, nullptr, 0, nullptr);
    }
    else if(event_handler_set){
      esp_event_isr_post(INPUT_EVENTS, pin, nullptr, 0, nullptr);
    }

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
    int gpio_level = gpio_get_level(minterrupt_args.mpin);
    gpio_level = (mactive_low ? !gpio_level : gpio_level);
    ESP_LOGD(GPIO_TAG, "read gpio %d pin: %d, active_low: %d", 
        minterrupt_args.mpin, gpio_level, mactive_low);
    return gpio_level;
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

  esp_err_t GpioInput::set_event_handler(esp_event_handler_t handler){
    if(handler == nullptr){ return ESP_ERR_INVALID_ARG; }
    std::lock_guard<std::mutex> guard(mregistration_mutex);
    esp_err_t r = mclear_event_handlers();
    if(r != ESP_OK){ return r; }
    r = esp_event_handler_instance_register(INPUT_EVENTS, minterrupt_args.mpin, handler, nullptr, &mevent_instance);
    if(r == ESP_OK){
      taskENTER_CRITICAL(&mevent_change_mutex);
      minterrupt_args.mevent_handler_set = true;
      mevent_handler_set = true;
      mevent_handle = handler;
      taskEXIT_CRITICAL(&mevent_change_mutex);
    }
    return r;
  }

  esp_err_t GpioInput::set_event_handler(esp_event_loop_handle_t loop, esp_event_handler_t handler){
    if(handler == nullptr){ return ESP_ERR_INVALID_ARG; }
    std::lock_guard<std::mutex> guard(mregistration_mutex);
    esp_err_t r = mclear_event_handlers();
    if(r != ESP_OK){ return r; }
    r = esp_event_handler_instance_register_with(loop, INPUT_EVENTS, minterrupt_args.mpin, handler, nullptr, &mevent_instance);
    if(r == ESP_OK){
      taskENTER_CRITICAL(&mevent_change_mutex);
      mevent_handle = handler;
      minterrupt_args.mcustom_event_loop_handle = loop;
      minterrupt_args.mcustom_event_handler_set = true;
      taskEXIT_CRITICAL(&mevent_change_mutex);
    }
    return r;
  }

  void GpioInput::set_queue_handle(QueueHandle_t queue){
    std::lock_guard<std::mutex> guard(mregistration_mutex);
    if(mclear_event_handlers() != ESP_OK){ return; }
    taskENTER_CRITICAL(&mevent_change_mutex);
    minterrupt_args.mqueue_handle = queue;
    minterrupt_args.mqueue_enable = queue != nullptr;
    taskEXIT_CRITICAL(&mevent_change_mutex);
  }

  esp_err_t GpioInput::mclear_event_handlers(){
    taskENTER_CRITICAL(&mevent_change_mutex);
    const interrupt_args_t previous = minterrupt_args;
    minterrupt_args.mcustom_event_handler_set = false;
    minterrupt_args.mevent_handler_set = false;
    minterrupt_args.mqueue_handle = nullptr;
    minterrupt_args.mqueue_enable = false;
    mevent_handler_set = false;
    taskEXIT_CRITICAL(&mevent_change_mutex);
    esp_err_t r = ESP_OK;
    // Event registration may block, so keep it outside the ISR spinlock.
    if(previous.mcustom_event_handler_set){
      r = esp_event_handler_instance_unregister_with(previous.mcustom_event_loop_handle,
          INPUT_EVENTS, previous.mpin, mevent_instance);
    }
    else if(previous.mevent_handler_set){
      r = esp_event_handler_instance_unregister(INPUT_EVENTS, previous.mpin, mevent_instance);
    }
    if(r == ESP_OK){ mevent_instance = nullptr; }
    else{
      // Keep the still-registered instance available for the next retry.
      taskENTER_CRITICAL(&mevent_change_mutex);
      minterrupt_args = previous;
      mevent_handler_set = previous.mevent_handler_set;
      taskEXIT_CRITICAL(&mevent_change_mutex);
    }
    return r;
  }
} // Namespace GpioInterface
