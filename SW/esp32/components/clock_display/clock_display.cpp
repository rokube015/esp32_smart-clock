#include "clock_display.h"
#include <cstdio>

esp_err_t CLOCK_DISPLAY::init(){
  const esp_err_t result = e_paper.init();
  display_ready = result == ESP_OK || result == ESP_ERR_TIMEOUT;
  if(!display_ready){ e_paper.deinit(); }
  black_sprite.setColorDepth(1);
  if(black_sprite.createSprite(e_paper.get_display_resolution_width(), e_paper.get_display_resolution_height()) == nullptr){
    deinit();
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK; // Panel errors do not prevent measurement or HTTP access.
}

void CLOCK_DISPLAY::deinit(){
  display_ready = false;
  black_sprite.deleteSprite();
  e_paper.deinit();
}

esp_err_t CLOCK_DISPLAY::render(const SensorData& data, const Context& context){
  if(!display_ready){ return ESP_ERR_INVALID_STATE; }
  const uint16_t co2 = data.co2;
  const float temperature = data.temperature;
  const double humidity = data.humidity;
  const float pressure = data.pressure;
  esp_err_t r = ESP_OK;
  if(context.provisioning){
    const char* setup_ssid = context.setup_ssid;
    const char* provisioning_pop = context.provisioning_pop;
    black_sprite.fillScreen(WHITE);
    black_sprite.setTextWrap(false);
    black_sprite.setTextColor(BLACK);
    black_sprite.setFont(&fonts::Font4);
    black_sprite.setTextSize(2);
    black_sprite.setCursor(32, 24);
    black_sprite.println("Wi-Fi setup");
    black_sprite.setTextSize(1);
    black_sprite.setCursor(32, 88);
    black_sprite.println("Connect your phone to this setup network:");
    black_sprite.setCursor(32, 122);
    black_sprite.printf("SSID: %s", setup_ssid);
    black_sprite.setCursor(32, 156);
    black_sprite.println("AP password: none (open network)");
    black_sprite.setCursor(32, 205);
    black_sprite.println("In Espressif Provisioning, select SoftAP.");
    black_sprite.setCursor(32, 239);
    black_sprite.println("Enter this proof of possession (PoP):");
    black_sprite.setTextSize(3);
    black_sprite.setCursor(32, 278);
    black_sprite.println(provisioning_pop);
    black_sprite.setTextSize(1);
    black_sprite.setCursor(32, 385);
    black_sprite.println("Then enter your 2.4 GHz Wi-Fi SSID and password.");
  }
  else{
    const char* time_info = context.time_valid ? context.time : "--:--";
    const char* day_info = context.time_valid ? context.day : "Time not synchronized";
    char display_buffer[50] = "\0";
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t x1 = 0;

    black_sprite.fillScreen(WHITE);
    black_sprite.setTextWrap(false);
    black_sprite.setTextColor(BLACK);
    black_sprite.setCursor(0, 0);
    black_sprite.setTextSize(2);
    black_sprite.setFont(&fonts::Font8);
    x = black_sprite.width()/2 - black_sprite.textWidth(time_info)/2;
    y = 10;
    black_sprite.setCursor(x, y);
    black_sprite.printf("%s\n", time_info);
    black_sprite.setFont(&fonts::FreeSans24pt7b);
    black_sprite.setTextSize(1);
    x = black_sprite.width()/2 - black_sprite.textWidth(day_info)/2;
    y = black_sprite.getCursorY() + 30;
    black_sprite.setCursor(x, y);
    black_sprite.printf("%s\n", day_info);
    black_sprite.setFont(&fonts::FreeSans24pt7b);
    black_sprite.setTextSize(1.5);
    if(data.co2_valid){ snprintf(display_buffer, sizeof(display_buffer), "CO2  %dppm\n", co2); }
    else{ snprintf(display_buffer, sizeof(display_buffer), "CO2  --ppm\n"); }
    x = black_sprite.width()/2 - black_sprite.textWidth(display_buffer)/2;
    y = black_sprite.getCursorY();
    y += 30; 
    black_sprite.setCursor(x, y);
    black_sprite.printf("%s", display_buffer);
    black_sprite.setTextSize(1);
    x = black_sprite.getCursorX();
    y = black_sprite.getCursorY() + 10;
    black_sprite.printf("Temperature");
    x1 =x;
    x = black_sprite.getCursorX();
    black_sprite.setCursor(x1, y + black_sprite.fontHeight());
    if(data.bme_valid()){ black_sprite.printf("%.1lf", temperature); }
    else{ black_sprite.print("--"); }
    black_sprite.setFont(&fonts::lgfxJapanGothicP_24);
    black_sprite.setTextSize(2);
    black_sprite.printf("\u2103");
    black_sprite.setFont(&fonts::FreeSans24pt7b);
    black_sprite.setTextSize(1);
    x += 50; 
    black_sprite.setCursor(x, y);
    black_sprite.printf("Humidity");
    x1 = x; 
    x = black_sprite.getCursorX();
    black_sprite.setCursor(x1, y + black_sprite.fontHeight());
    if(data.bme_valid()){ black_sprite.printf("%.2lf%%\n", humidity); }
    else{ black_sprite.print("--%\n"); }
    x += 50; 
    black_sprite.setCursor(x, y);
    black_sprite.printf("Pressure");
    x1 = x; 
    x = black_sprite.getCursorX();
    black_sprite.setCursor(x1, y + black_sprite.fontHeight());
    if(data.bme_valid()){ black_sprite.printf("%.2lfhpa\n", pressure); }
    else{ black_sprite.print("--hpa\n"); }
  }

  if(!context.provisioning){
    black_sprite.setFont(&fonts::Font4);
    black_sprite.setTextSize(1);
    black_sprite.setCursor(12, 450);
    if(context.downloads && context.connected){ black_sprite.printf("Data: http://%s:8080", context.address); }
    else{ black_sprite.print(context.connected ? "Wi-Fi connected" : "SW4: connect to Wi-Fi"); }
  }
  if(r == ESP_OK){
    if(e_paper.get_state() == EPAPER4IN26::state_e::SLEEP ||
       e_paper.get_state() == EPAPER4IN26::state_e::NOT_INITIALIZED){
      r = e_paper.init_epaper();
    }
    if(r == ESP_OK){ r = e_paper.display((uint8_t*)black_sprite.getBuffer(), e_paper.get_display_bytes()); }
    if(r == ESP_OK){ r = e_paper.set_sleep_mode(); }
  }
  return r;
}
