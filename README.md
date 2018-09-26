# Matrix-Voice-ESP32-MQTT-Audio-Streamer

The Audio Steamer is designed to work as an Snips Audio Server, see https://snips.ai/
Feel free to make comments.

I have switched from c++ to Arduino IDE, because there were better solutions for the MQTT stuff.
In the folder "MatrixVoiceAudioServerArduino", there are three bin files:
- bootloader.bin
- partitions_singleapp.bin
- MatrixVoiceAudioServerArduino.ino.esp32.bin

The first two I copied directly from the build/bootloader and build folders from the ESP32 demo.
If you want to build and use your own, get the ESP32 demo running:
https://www.hackster.io/matrix-labs/get-started-w-esp32-on-the-matrix-voice-d01e0d
In the folder mic_energy/build you will find the files I uses.

The third is a compiled version.

## Features

- Runs standalone, NO raspberry Pi is needed after flashing this program
- Ledring starts RED (no wifi connection), then turns BLUE (Idle mode, with wifi connection)
- Ledring turns GREEN when the hotword is detected, back to BLUE when going to idle
- Uses an asynchronous MQTT client for the playBytes topic (large messages which cannot be handled my synchronous clients)
- Uses a synchronous MQTT client for the audiostream.

## Get started

To get the code running I suggest you first reset the Voice if you have flashed it previously

- Connect your Voice with a Raspberry Pi
- ssh into the pi, execute this command: voice_esp32_enable. If you get a permission denied, execute the command again. 
- esptool.py --chip esp32 --port /dev/ttyS0 --baud 115200 --before default_reset --after hard_reset erase_flash
- Reboot the Pi.

Start your Arduino IDE, copy the folder "hal" to your libraries folder.
You will also need these libaries:
- https://github.com/marvinroger/async-mqtt-client
- https://github.com/me-no-dev/AsyncTCP
- hhttps://github.com/knolleary/pubsubclient
- Change the MQTT_IP, MQTT_PORT, MQTT_HOST, SITEID, SSID and PASSWORD to fit your needs
- Change the MQTT_MAX_PACKET_SIZE in PubSubClient.h to 2000: See https://github.com/knolleary/pubsubclient (limitations)
- Compile => Do an "Export compiled libary"! This will overwrite the "MatrixVoiceAudioServerArduino.ino.esp32.bin" file
- In the file deploy.sh, change to IP address to the IP your Pi is on.
- Open a terminal (Linux and Mac should work, I use Mac), change to the folder where this code is and run: ./deploy.sh
- If you have done the ESP32 demo, you should see the familiar: "esptool.py wrapper for MATRIX Voice" messages.
- Watch how it flashes and when it restarts, the ledring should light up: first red (wifi disconnected), then blue (connected)
- When your hotword is detected, it should become green and return to blue after command or timeout

## OTA c++ version

The OTA version is not maintained at the moment, but I might in the future get it going again. 
The reason is that I have put a lot of time in getting the playBytes to work on the ESP32. Wave files are published to that topic and I did a lot of research and have implemented an asynchronus MQTT client for this. Otherwise the voice would choke and reboot on those large messages due to limited memory.
The last commited version will probably NOT work.
I am working on an OTA version via the Arduino IDE

## Known issues
- Sometimes not all leds light up, but only 2/3 or so. Does not seem to have any effect the functioning.
