#pragma once
#include "sensor_data.h"
#include "e_paper.h"
#include "LovyanGFX.hpp"

class CLOCK_DISPLAY {
public:
  struct Context {
    bool provisioning {false};
    bool time_valid {false};
    bool connected {false};
    bool downloads {false};
    char setup_ssid[33] {};
    char provisioning_pop[17] {};
    char time[50] {"--:--"};
    char day[50] {"Time not synchronized"};
    char address[24] {};
  };
  esp_err_t init();
  void deinit();
  esp_err_t render(const SensorData& data, const Context& context);
private:
  EPAPER4IN26 e_paper;
  LGFX_Sprite black_sprite;
  bool display_ready {false};
  static constexpr uint8_t WHITE = 255, BLACK = 0;
};
