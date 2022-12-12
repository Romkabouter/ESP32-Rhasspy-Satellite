## Get Started for INMP441, MAX98357a and FastLED
- Set the device_type to 7 in the settings.ini
- Configure pins on /devices/Inmp441Max98357aFastLed.hpp
- Flash the esp32 using usb to uart converter
- First flash, set "upload" as method under the OTA section in the settings.ini. After that you can set it to "ota" as well
- Build and flash the project with PlatformIO, for example run `pio run --target upload` from the commandline


## More info
https://github.com/ProrokWielki/HomeAssistantSpeaker