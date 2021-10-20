## Get Started for ESP32 POE ISO
- Set the device_type to 5 in the settings.ini
- Connect the device to your computer via USB, like a regular esp32
- First flash, set "upload" as method under the OTA section in the settings.ini. After that you can set it to "ota" as well
- Build and flash the project with PlatformIO, for example run `pio run --target upload` from the commandline
