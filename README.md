# Matrix-Voice-ESP32-MQTT-Audio-Streamer

The Audio Steamer is designed to work as an Snips Audio Server, see https://snips.ai/
Feel free to make comments, I am not a c / c++ programmer so a lot well be not very neat programming.

To be able to run it, first try to get the demo esp32 of Matrix running, see https://www.hackster.io/matrix-labs/get-started-w-esp32-on-the-matrix-voice-d01e0d
If you have got that running, you will be able to flash this software as well.

I have implemented OTA, for which I have used this repo: https://github.com/classycodeoss/esp32-ota
To be able to do that, follow these steps:

- Connect your Voice with a Raspberry Pi, you should probably already have that when you were trying the esp32 demo bij Hackster.io
- ssh into the pi, execute this command: voice_esp32_enable. If you get a permission denied, execute the command again. 
- Pull the plug on the Pi, and restart it.
- In this repo, do a make menuconfig, go to the Partition Table entry and change it to "Factory App, two OTA definitions"
- Enter MatrixVoiceAudioServer and enter the credentials. I have not tried it with a mqtt password, but it should work.
- Exit the config and edit the make/deploy.mk file. Change to IP address to the IP your Pi is on.
- make and make deploy

After the flashing, to led ring should start with RED, and when connected to your wifi, it should become BLUE.
If connected to the MQTT broker your Snips is listening to, the ledring should become GREEN when the hotword is detected and then BLUE again.
Test out the OTA flashing by doing: python update_firmware.py <ipadress-of-your-espvoice> build/MatrixVoiceAudioServer.bin
The ledring should become WHITE and some console should be printed, see https://github.com/classycodeoss/esp32-ota

When all this was working correctly, you can unplug the Pi, remove the Voice and plugin the adapter to the Voice
While booting, the same should happen: RED when initializing, then BLUE when connected.
