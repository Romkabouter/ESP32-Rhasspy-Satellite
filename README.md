# ESP32 Rhasspy Satellite

The ESP32 Audio Steamer is designed to work as a satellite for Rhasppy (https://rhasspy.readthedocs.io/en/latest/)
Please raise an issue if some of the steps do not work or if they are unclear.

## Features

- Supports multiple devices, and you are welcomed to add more devices. Read futher below as to how.
- For the Matrix Voice, during first flash a Raspberri Pi is needed, afer that OYA can be used
- LED Support
- OTA Updating
- Dynamic brightness and colors for idle, hotword, updating and disconnected
- Mute / unmute microphones via MQTT
- Reboot device by sending hashed password

## Get started for Matrix Voice

To get the code running I suggest you first reset the Voice if you have flashed it previously

- Follow Step 1 and 2 (or all steps) from this guide https://matrix-io.github.io/matrix-documentation/matrix-voice/esp32/
- Once your environment is set up go into the PlatformIO directory
- Copy and configure the `settings.ini.example` to `settings.ini` and configure the proper parameters. Any `$` in the `settings.ini` will be threated as an environment variable, did not find a way to escape it yet, fixes or workarounds are welcome
- Set the device_type to 1 in the settings.ini
- Build the project with PlatformIO, for example run `pio run` from the commandline
- Remotely flash the bin file using the provided script `sh deploy.sh pi@raspberry_ip_address`
- From this point one the MatrixVoice can run standalone and you should be able to update via OTA by running `pio run --target upload`.
  Use "ota" as method under the OTA section in the settings.ini
- If you change code and OTA does not work anymore for some reason, you can always start over by doing the "get started" part except for the first bullet

## Get Started for M5 Atom Echo
- Set the device_type to 0 in the settings.ini
- First flash, set "upload" as method under the OTA section in the settings.ini. After that you can set it to "ota" as well
- Build and fash the project with PlatformIO, for example run `pio run --target upload` from the commandline

## MQTT commands

The ESP32 Satellite is subscribed to various topics.
The topic SITEID/led, where SITEID is the name you have given the device in settings.ini, is used for commands concerning the leds on the device.
When publishing to this topic, the led colors and brightness can be altered without coding, setting it to retained will update the settings as soon as the device is connected.
The message can contain 7 keys:

- brightness: integer value between 0 and 100 (%)
- hotword_brightness: integer value between 0 and 100 (%)
- idle: array of 4 codes: [red,green,blue,white], ranging 0-255
- hotword: array of 4 codes: [red,green,blue,white], ranging 0-255
- update: array of 4 codes: [red,green,blue,white], ranging 0-255
- wifi_disconnect: array of 4 codes: [red,green,blue,white], ranging 0-255
- wifi_connect: array of 4 codes: [red,green,blue,white], ranging 0-255

Example: {"brightness":20,"idle":[240,210,17,0],"hotword":[173,17,240,0],"update":[0,255,255,0]}

The topic SITEID/audio also can receive multiple commands:

- Mute/unmute microphones: publish {"mute_input":"true"} or {"mute_input":"false"}
- Mute/unmute playback: publishing {"mute_output":"true"} or {"mute_output":"false"}
- Change the amp to jack/speaker: publish {"amp_output":"0"} or {"amp_output":"1"} (Only if a device supports this)
- Adjust mic gain: publish {"gain":5}
- Switch local/remote hotword detection: publish {"hotword":"local"} or {"hotword":"remote"}, Local only supports "Alexa"
- Adjust volume: publish {"volume": 50} (If device supports this)

Restart the device by publishing {"passwordhash":"yourpasswordhash"} to SITEID/restart

## Known issues

- Uploading sometimes fails or an error is thrown when the uploading is done.
- Audio playback with sample rate > 22050 can lead to hissing/cracking/distortion. Recommended is to use a samplerate of 16000

# Adding devices

It is possible to add device that have leds. micrphones and speakers, the statemachine does not have to change for this.

Adding a device is relatively simple:

- Update settings.ini.example and give your device a uppercase name and a number
- Update Satellite.cpp and add a #define with the same name and number you created in the settings.
- Create a hpp file for your device in the devices folder and implement the methods you need, see the other devices for examples.
- Add a #ifdef in Satellite.cpp as per examples already there
- Add needed libraries in platform.ini under lib_deps, they will be compiled
- Search for examples in the code ore raise an issue if you need help