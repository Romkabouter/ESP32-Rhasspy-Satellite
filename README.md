# Matrix-Voice-ESP32-MQTT-Audio-Streamer

The Audio Steamer is designed to work as an Snips Audio Server, see https://snips.ai/ and also support Rhasppy (https://rhasspy.readthedocs.io/en/latest/)
Please raise an issue if some of the steps do not work or if they are unclear.
The Arduino code is no longer maintained and will not be futher developed, I found platformIO the better choise.

## Features

- Runs standalone, NO raspberry Pi is needed after flashing this program
- Ledring starts RED (no wifi connection), then turns BLUE (Idle mode, with wifi connection)
- Ledring turns GREEN when the hotword is detected, back to BLUE when going to idle
- Uses an asynchronous MQTT client for the playBytes topic
- Uses a synchronous MQTT client for the audiostream.
- OTA Updating
- Dynamic brightness and colors for idle, hotword, updating and disconnected
- Mute / unmute microphones via MQTT
- Reboot device by sending hashed password
- Resampling mono/stereo to 44100 stereo using Speex library (only platformIO)

## Get started (PlatformIO, recommended)

To get the code running I suggest you first reset the Voice if you have flashed it previously

- Follow Step 1 and 2 (or all steps) from this guide https://matrix-io.github.io/matrix-documentation/matrix-voice/esp32/
- ssh into the pi, execute this command: voice_esp32_enable. If you get a permission denied, execute the command again. 
- esptool.py --chip esp32 --port /dev/ttyS0 --baud 115200 --before default_reset --after hard_reset erase_flash
- Reboot the Pi.
- Follow this guide on HackterIO to get started: https://www.hackster.io/matrix-labs/program-matrix-voice-esp32-with-vs-code-using-platformio-3dd498
- After that you can use OTA, the platformIO is more stable then the Arduino version

## Ardiuno (depricated)

In the folder "MatrixVoiceAudioServer", there are two bin files:
- bootloader.bin
- partitions_two_ota.bin
These files are needed in order to do the first flashing. You can build your own versions of it by checking out the OTABuilder folder.
In OTABuilder is a program in c++, which does nothing but if you do a make menuconfig you will see that the partition is set to OTA.
When you do a make, the partitions_two_ota.bin will be in the build folder and the bootloader.bin in the build/bootloader folder.

To flash the OTA version for the first time, attach the Voice to a Raspberry Pi. 
- Get the HAL code from https://github.com/matrix-io/matrixio_hal_esp32/tree/master/components/hal
- Copy the folder "hal" to your Arduino IDE libraries folder
- Add to Arduino IDE: AsynchMqttClient https://github.com/marvinroger/async-mqtt-client
- Be sure to check this pull request: https://github.com/marvinroger/async-mqtt-client/pull/117
  This code uses disconnect(), which will cause the MatrixVoice to crash if the fix from the PR is not in it.
- Download and add to Arduino IDE: https://github.com/me-no-dev/AsyncTCP
- Add to Arduino IDE: PubSubClient by Nick O'Leary https://github.com/knolleary/pubsubclient
- Add to Arduino IDE: ArduinoJson.
- Open the MatrixVoiceAudioServer.ino in the Arduino IDE
- Select ESP32 Dev Module as Board, set flash size to 4MB and Upload speed to 115200
- Change the MQTT_IP, MQTT_PORT, MQTT_HOST, SITEID, MQTT_USER, MQTT_PASS, SSID and PASSWORD to fit your needs. MQTT_USER, MQTT_PASS, SSID and PASSWORD are in config.h
- Change the MQTT_MAX_PACKET_SIZE in PubSubClient.h to 2000: See https://github.com/knolleary/pubsubclient (limitations)
- Compile => Do an "Export compiled libary"
- In the file deploy_ota.sh, change to IP address to the IP your Pi is on.
- Open a terminal (Linux and Mac should work, I use Mac), change to the folder where this code is and run: sh deploy_ota.sh
- If you have done the ESP32 demo, you should see the familiar: "esptool.py wrapper for MATRIX Voice" messages.
- Watch how it flashes and when it restarts, the ledring should light up: first red (wifi disconnected), then blue (connected)
- Shutdown the Pi and Arduino IDE
- Remove the voice from the Pi and plug the power into the Voice with a micro usb cable, the Voice should start
- Open your Arduino IDE again, after a while the Matrix Voice should show up as a network port (Tools->Port), select this port
- Make a change (or not) and do a Sketch -> Upload. The leds will turn WHITE
- Sometimes uploading fails, just retry untill it succeeds.

If you change code and OTA does not work anymore for some reason, you can always start over by doing the "get started" part except for the first bullit 

## MQTT commands

The Matrix Voice Audio Server is subscribed to various topics.
The topic SITEID/everloop, where SITEID is the name you have given the device, is used for commands concerning the ledring.
When publishing to this topic, the everloop colors and brightness can be altered without coding, setting it to retained will update the settins as soon as the device is connected.
The message can contain 4 keys:
 - brightness: integer value between 0 and 100 (%)
 - idle: array of 4 codes: [red,green,blue,white], ranging 0-255
 - hotword: array of 4 codes: [red,green,blue,white], ranging 0-255
 - update: array of 4 codes: [red,green,blue,white], ranging 0-255
 - disconnect: array of 4 codes: [red,green,blue,white], ranging 0-255

Example: {"brightness":20,"idle":[240,210,17,0],"hotword":[173,17,240,0],"update":[0,255,255,0]}

Restart the device by publishing {"passwordhash":"yourpasswordhash"} to the topic SITEID/restart 
Mute/unmute the microphones by publishing {"mute_input":"true"} or {"mute_input":"false"} to the topic SITEID/audio
Mute/unmute playback by publishing {"mute_output":"true"} or {"mute_output":"false"} to the topic SITEID/audio
Change the amp to jack/speaker by publishing {"amp_output":0} or {"amp_output":1} to the topic SITEID/audio
Adjust mic gain publishing {"gain":5} to the topic SITEID/audio
You can also change the framesize by sending commands to SITEID/audio

## Roadmap

These features I want to implement in the future, not ordered in any way
- led animations
- 3D case including small speakers.

## Known issues
- Uploading a sketch sometimes fails or an error is thrown when the uploading is done. If you get the error, check if your new sketch has been implemented or start over.
