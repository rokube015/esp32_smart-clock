#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "sntp_interface.h"

class MAIN final{
  private:
  public:
    void run(void);
    void setup(void);

    WIFI::state_e wifi_state {WIFI::state_e::NOT_INITIALIZED};
    WIFI wifi;
    SNTP sntp;
}; 
