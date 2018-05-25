/* ************************************************************************* *
 * Matrix Voice Audio Streamer
 * 
 * This program is written to be a streaming audio server running on the Matrix Voice.
 * It can be configured as a raw audio streamer to a MQTT broker, but can also wrap each message
 * in a wave container. This is typically used for Snips.AI, it will then be able to replace
 * the Snips Audio Server, by publishing small wave messages to the hermes proticol
 * See https://snips.ai/ for more information
 * 
 * The MQTT code is based upon the code written by Jesse Braham
 * See https://github.com/jessebraham/esp32-mqtt-client for this part of the code.
 *
 * Author:  Paul Romkes
 * Date:    May 2018
 * Version: 1
 * 
 * Changelog:
 * ==========
 * v1:
 *  - first code release. It needs a lot of improvement, no hardcoding stuff
 * v2:
 *  - rewrite to support OTA (Over the air Update)
 * ************************************************************************ */

#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
    #include <string.h>
    #include "esp_mqtt.h"
    #include "network.h"
    #include "ota_update.h"
}

#include "everloop.h"
#include "everloop_image.h"
#include "voice_memory_map.h"
#include "microphone_array.h"
#include "wishbone_bus.h"

// *****************************************************
// TODO - update these definitions for your environment!
// *****************************************************
#define AP_SSID CONFIG_WIFI_SSID
#define AP_PASSWORD CONFIG_WIFI_PASS
#define SITEID CONFIG_SITEID
#define RATE 16000
#define CHUNK 256 //set to multiplications of 256, voice return a set of 256
#define WIDTH 2
#define CHANNELS 1

namespace hal = matrix_hal;
hal::MicrophoneArray mics;
hal::Everloop everloop;
hal::EverloopImage image1d;

uint16_t voicebuffer[CHUNK];
uint8_t voicemapped[CHUNK*WIDTH];

struct wavfile_header {
	char	riff_tag[4];
	int	    riff_length;
	char	wave_tag[4];
	char	fmt_tag[4];
	int	    fmt_length;
	short	audio_format;
	short	num_channels;
	int	    sample_rate;
	int	    byte_rate;
	short	block_align;
	short	bits_per_sample;
	char	data_tag[4];
	int	    data_length;
};
   
void setEverloop(int red, int green, int blue, int white) {
    for (hal::LedValue& led : image1d.leds) {
      led.red = red;
      led.green = green;
      led.blue = blue;
      led.white = white;
    }

    everloop.Write(&image1d);
}

int cpp_loop(void)
{
    struct wavfile_header header;

    strncpy(header.riff_tag,"RIFF",4);
    strncpy(header.wave_tag,"WAVE",4);
    strncpy(header.fmt_tag,"fmt ",4);
    strncpy(header.data_tag,"data",4);

    header.riff_length = (uint32_t)sizeof(header) + (CHUNK * WIDTH);
    header.fmt_length = 16;
    header.audio_format = 1;
    header.num_channels = 1;
    header.sample_rate = RATE;
    header.byte_rate = RATE * WIDTH;
    header.block_align = WIDTH;
    header.bits_per_sample = WIDTH * 8;
    header.data_length = CHUNK * WIDTH;

    nvs_flash_init();
    // Connect to an access point to receive OTA firmware updates.
    networkInit();
    networkConnect(AP_SSID, AP_PASSWORD);
    
    // ---------------------------------------------------------------------------
    // MATRIX VOICE STUFF
    // ---------------------------------------------------------------------------
    hal::WishboneBus wb;
    wb.Init();
    //setup everloop
    everloop.Setup(&wb);
    setEverloop(0,0,0,0);
    //setup mics
    mics.Setup(&wb);
    mics.SetSamplingRate(RATE);
    mics.ReadConfValues();
    //Hmm, is this actually needed and what does it do?
    mics.CalculateDelays(0, 0, 1000, 320 * 1000);  

    setEverloop(10,0,0,0);
  
    while (true) {
        if (otaUpdateInProgress()) {
            //updating: WHITE
            setEverloop(0,0,0,10);
        }
        if (networkIsConnected() && !otaUpdateInProgress() && !HotwordDetected()) {
            //connected, not updating but hotword not detected BLUE
            setEverloop(0,0,10,0);
        }
        if (networkIsConnected() && HotwordDetected()) {
            //connected and hotword detected GREEN
            setEverloop(0,10,0,0);
        }
        if (!networkIsConnected()) {
            //not connected: RED
            setEverloop(10,0,0,0);
        }
        if (mqttIsConnected()) {
            //We are connected! Read the mics
            mics.Read();
        
            //NumberOfSamples() = kMicarrayBufferSize / kMicrophoneChannels = 2048 / 8 = 256
            //These are 256 samples of 2 bytes
            for (uint32_t s = 0; s < mics.NumberOfSamples(); s++) {
                voicebuffer[s] = mics.Beam(s);
            }
        
            //voicebuffer will hold 256 samples of 2 bytes, but we need it as 1 byte
            //We do a memcpy, because I need to add the wave header as well
            memcpy(voicemapped,voicebuffer,CHUNK*WIDTH);
        
            uint8_t payload[sizeof(header)+(CHUNK*WIDTH)];
            //Add the wave header
            memcpy(payload,&header,sizeof(header));
            memcpy(&payload[sizeof(header)], voicemapped, sizeof(voicemapped));
    
            //Ok, we can now  send the wave message
            std::string topic = std::string("hermes/audioServer/") + SITEID + std::string("/audioFrame");

            esp_mqtt_publish(topic.c_str(), (uint8_t *)payload, sizeof(payload),0, false);
        }
    }    
}

extern "C" {
   void app_main(void) {cpp_loop();}
}