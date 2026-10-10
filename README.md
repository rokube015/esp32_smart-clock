# ESP32-smart-clock

This repository provides firmware and PCB design for a clock with environmental monitoring project using the ESP-IDF framework.

## Features
- CO2, temperature, humidity and atmosphere pressure monitoring.
- Custom PCB design for the project.
  
<img src="./images/Photo_1.JPG" width="300" /> <img src="./images/Photo_2.JPG" width="300" />
## Prerequisites

### Software Requirements
- ESP-IDF version 6.1 (ESP32-S3 target)
- Python 3.x
## Setup
1. **Install ESP-IDF**: Follow the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).
2. **Clone the Repository**:
   ```bash
   git clone https://github.com/rokube015/esp32_smart-clock.git
   ```
3. **Configure Wi-Fi on first boot**: Flash and start the device, then use Espressif's SoftAP Provisioning app to connect to the `SMART_CLOCK_<MAC>` setup network. Enter the proof of possession (PoP) printed in the serial log, then select your Wi-Fi network and enter its password in the app. The credentials are stored by ESP-IDF in NVS and reused on later boots. The setup access point is open, but provisioning messages are encrypted using the one-time PoP.

   If connection to the saved network fails repeatedly, the device starts SoftAP provisioning again so you can configure a new network.
## License
This project is licensed under the [Apache License 2.0](./LICENSE).

### Third-Party Licenses

This project utilizes the following third-party libraries:

- **[ESP-IDF](https://github.com/espressif/esp-idf)** - Licensed under the Apache License 2.0
- **[FreeRTOS](https://github.com/FreeRTOS/FreeRTOS)** - Licensed under the MIT License
- **[LovyanGFX](https://github.com/lovyan03/LovyanGFX)** - Licensed under the MIT License

Each library is subject to its own license terms. Please refer to the respective repositories for more information.
## Contributing
Contributions are welcome!
Please use the Issues tab to report bugs or suggest features, and feel free to submit pull requests.
