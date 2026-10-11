#pragma once

#include "gpio_interface.h"
#include "button_debounce.h"
#include "freertos/event_groups.h"

class BUTTON {
public:
  enum class EventType { PRESSED };
  struct Event {
    EventType type;
    gpio_num_t pin;
    TickType_t tick;
  };
  esp_err_t init(gpio_num_t pin, QueueHandle_t destination);
  esp_err_t create_task(const char* name, uint16_t stack_size, UBaseType_t priority, EventGroupHandle_t gate = nullptr);
  void deinit();

private:
  GpioInterface::GpioInput input;
  button_control::DebouncedButton debounce {pdMS_TO_TICKS(40)};
  QueueHandle_t event_queue {nullptr}; // Owned by SMART_CLOCK.
  EventGroupHandle_t startup_gate {nullptr};
  TaskHandle_t task_handle {nullptr};
  gpio_num_t input_pin {GPIO_NUM_NC};
  static void task_entry(void* arg);
  void monitor_task();
};
