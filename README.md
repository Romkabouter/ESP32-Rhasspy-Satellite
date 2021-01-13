# ESP32 Rhasspy Satellite

The ESP32 Audio Steamer is designed to work as a satellite for Rhasppy (https://rhasspy.readthedocs.io/en/latest/)
Please raise an issue if some of the steps do not work or if they are unclear.
Support for Snips is dropped.

## Features

- Supports multiple devices, and you are welcomed to add more devices. Read futher below as to how.
- For the Matrix Voice, during first flash a Raspberri Pi is needed, afer that OTA can be used
- LED Support
- OTA Updating
- Dynamic brightness and colors for idle, hotword and disconnected
- Mute / unmute microphones via MQTT
- Mute / unmute speakers via MQTT
- Adjust volume via MQTT (if supported by device)
- Adjust output (speaker/jack) via MQTT (if supported by device)
- Adjust gain via MQTT (if supported by device)
- Reboot device by sending hashed password
- Configuration possible in browser
- Audio playback, recommended not higher than 441000 samplerate (see Known Issues)
- Hardware button to start session (if supported by device)

## Getting started
[Matrix Voice](matrixvoice.md)

[M5 Atom Echo](m5atomecho.md)

[Audio Kit](audiokit.md)

## MQTT commands

The ESP32 Satellite is subscribed to various topics.
The topic SITEID/led, where SITEID is the name you have given the device in settings.ini, is used for commands concerning the leds on the device.
When publishing to this topic, the led colors and brightness can be altered without coding and will be saved to a config file

The message can contain 7 keys:

- brightness: integer value between 0 and 100 (%)
- hotword_brightness: integer value between 0 and 100 (%)
- idle: array of 4 codes: [red,green,blue,white], ranging 0-255
- hotword: array of 4 codes: [red,green,blue,white], ranging 0-255
- update: array of 4 codes: [red,green,blue,white], ranging 0-255
- wifi_disconnect: array of 4 codes: [red,green,blue,white], ranging 0-255
- wifi_connect: array of 4 codes: [red,green,blue,white], ranging 0-255

Example: {"brightness":20,"idle":[240,210,17,0],"hotword":[173,17,240,0]}

The topic SITEID/audio also can receive multiple commands:

- Mute/unmute microphones: publish {"mute_input":"true"} or {"mute_input":"false"}
- Mute/unmute playback: publishing {"mute_output":"true"} or {"mute_output":"false"}
- Change the amp to jack/speaker: publish {"amp_output":"0"} or {"amp_output":"1"} (Only if a device supports this)
- Adjust mic gain: publish {"gain":5}
- Adjust volume: publish {"volume": 50} (If device supports this)

Restart the device by publishing {"passwordhash":"yourpasswordhash"} to SITEID/restart

## Known issues

- Uploading sometimes fails or an error is thrown when the uploading is done.
- Audio playback with sample rate higher than 22050 can lead to hissing/cracking/distortion. Recommended is to use a samplerate of 16000
- Audio playback with matrix voice is not good, code needs to resample to 44100. WIP
- Update colors do not work yet

# Adding devices

It is possible to add device that have leds. micrphones and speakers, the statemachine does not have to change for this.

Adding a device is relatively simple:

- Update settings.ini.example and give your device a uppercase name and a number
- Update Satellite.cpp and add a #define with the same name and number you created in the settings.
- Create a hpp file for your device in the devices folder and implement the methods you need, see the other devices for examples.
- Add a #ifdef in Satellite.cpp as per examples already there
- Add needed libraries in platform.ini under lib_deps
- Search for examples in the code or raise an issue/quastion if you need help
- Add a "get started" md file and link it in the readme.