## Get Started for Audio Kit
- Set the device_type to 2 in the settings.ini
- Connect the Audio Kit to your computer via USB, like a regular esp32
- First flash, set "upload" as method under the OTA section in the settings.ini. After that you can set it to "ota" as well
- Build and flash the project with PlatformIO, for example run `pio run --target upload` from the commandline

# Supported features
- All known hardware variants (AC101, ES8388 pinout variants 1 and 2) supported
- Optional command activation by key press on key 1
- Single LED state notification using D4
- LED brightness control
- Configurable output to headphone jack and/or speakers

# Unsupported features
- Configurable LED animations 
- Configurable microphone input gain

# LED Notification
The device indicates its various states through a single LED using different patterns.

| LED                             | State                                     |
|---------------------------------|-------------------------------------------|
| Slow flashing (2s cycle)        | WiFi not connected / startup              |
| Fast flashing (1s cycle)        | WiFi connected, but no MQTT connection    |
| Off                             | Ready, listening for hotword / activation |
| Slow pulsing  (4s cycle)        | Listening to voice commands               |
| Steady on                       | Speech output from Rhasspy                |
| Very fast flashing (0.5s cycle) | Voice command not recognized              |  
| Fast pulsing (0.5s cycle)       | Over-the-air update in progress           |

# Known issues
- AC101 based boards may not go back easily into hotword detection after voice command, they keep listening for voice commands. Reduce VAD sensistivity (i.e. set VAD to a higher value) in Rhasspy Speech to Text settings to mitigate.