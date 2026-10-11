#include "button.h"

esp_err_t BUTTON::init(gpio_num_t pin, QueueHandle_t destination){
  if(destination == nullptr){ return ESP_ERR_INVALID_ARG; }
  if(task_handle != nullptr){ return ESP_ERR_INVALID_STATE; }
  event_queue = nullptr;
  input_pin = pin;
  esp_err_t result = input.init(pin, true);
  if(result == ESP_OK){ result = input.enable_pull_up(); }
  if(result == ESP_OK){ input_pin = pin; event_queue = destination; }
  return result;
}

esp_err_t BUTTON::create_task(const char* name, uint16_t stack_size, UBaseType_t priority, EventGroupHandle_t gate){
  if(event_queue == nullptr || task_handle != nullptr){ return ESP_ERR_INVALID_STATE; }
  startup_gate = gate;
  return xTaskCreate(task_entry, name, stack_size, this, priority, &task_handle) == pdPASS
      ? ESP_OK : ESP_ERR_NO_MEM;
}

void BUTTON::task_entry(void* arg){
  auto* instance = static_cast<BUTTON*>(arg);
  if(instance->startup_gate != nullptr){ xEventGroupWaitBits(instance->startup_gate, 1, pdFALSE, pdTRUE, portMAX_DELAY); }
  instance->monitor_task();
}

void BUTTON::monitor_task(){
  TickType_t last_wake = xTaskGetTickCount();
  while(true){
    const TickType_t now = xTaskGetTickCount();
    if(debounce.update(input.read() != 0, now)){
      const Event event {EventType::PRESSED, input_pin, now};
      ESP_LOGI("button", "IO%d pressed; sending PRESSED event", static_cast<int>(input_pin));
      xQueueSend(event_queue, &event, portMAX_DELAY);
    }
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(20));
  }
}

void BUTTON::deinit(){
  if(task_handle != nullptr){ vTaskDelete(task_handle); task_handle = nullptr; }
  event_queue = nullptr;
  startup_gate = nullptr;
  if(input_pin != GPIO_NUM_NC){ gpio_reset_pin(input_pin); input_pin = GPIO_NUM_NC; }
}
