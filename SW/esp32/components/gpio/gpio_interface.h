#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "esp_log.h"

namespace GpioInterface{
  ESP_EVENT_DECLARE_BASE(INTPUT_EVENTS);
  
  class GpioBase{
    protected:
      bool mactive_low = false;
      constexpr static const char* GPIO_TAG = "gpio_interface";
  }; //GpioBase Class

  class GpioInput : public GpioBase{
    private:
      esp_err_t minit(const gpio_num_t pin, const bool active_low);
      bool mevent_handler_set = false;
      static bool minterrupt_service_installed;

      esp_event_handler_t mevent_handle = nullptr;
      static portMUX_TYPE mevent_change_mutex;

      esp_err_t mclear_event_handlers();

      typedef struct interrupt_args{
        bool mevent_handler_set = false;
        bool mcustom_event_handler_set = false;
        bool mqueue_enable = false;
        gpio_num_t mpin;
        esp_event_loop_handle_t mcustom_event_loop_handle;
        QueueHandle_t mqueue_handle{nullptr};
      }interrupt_args_t;

      interrupt_args_t minterrupt_args;

    public:
      GpioInput(const gpio_num_t pin, const bool active_low);
      GpioInput(const gpio_num_t pin);
      GpioInput(void);
      esp_err_t init(const gpio_num_t pin, const bool active_low);
      esp_err_t init(const gpio_num_t pin);
      int read(void);

      esp_err_t enable_pull_up(void);
      esp_err_t disable_pull_up(void);
      esp_err_t enable_pull_down(void);
      esp_err_t disable_pull_down(void);
      esp_err_t enable_pull_up_and_pull_down(void);
      esp_err_t disable_pull_up_and_pull_down(void);

      esp_err_t enable_interrupt(gpio_int_type_t int_type);
      esp_err_t set_event_handler(esp_event_handler_t Gpio_e_h);
      esp_err_t set_event_handler(esp_event_loop_handle_t Gpio_e_l, esp_event_handler_t Gpio_e_h);
      void set_queue_handle(QueueHandle_t Gpio_e_q);

      static void IRAM_ATTR gpio_isr_callback(void* arg);
  }; // GpioInput Class

  class GpioOutput : public GpioBase{
    private:
      gpio_num_t mpin;
      int mlevel = 0;
      esp_err_t minit(const gpio_num_t pin, const bool active_low);

    public:
      GpioOutput(const gpio_num_t pin, const bool active_low);
      GpioOutput(const gpio_num_t pin);
      GpioOutput(void);
      esp_err_t init(const gpio_num_t pin, const bool active_low);
      esp_err_t init(const gpio_num_t pin);
      esp_err_t on(void);
      esp_err_t off(void);
      esp_err_t toggle(void);
      esp_err_t set_level(int level);
  };
}
