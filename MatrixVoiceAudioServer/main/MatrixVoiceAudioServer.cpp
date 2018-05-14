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
 * ************************************************************************ */

// ---------------------------------------------------------------------------
// INCLUDES 
// ---------------------------------------------------------------------------

extern "C" {
//    #include <stdlib.h>
    #include <string.h>

    #include "esp_event_loop.h"
    #include "esp_log.h"
    #include "esp_mqtt.h"
    #include "esp_system.h"
    #include "esp_wifi.h"

    #include "nvs_flash.h"
    #include "sdkconfig.h"
}

#include <fstream>

// ---------------------------------------------------------------------------
// INCLUDES for Matrix Voice
// ---------------------------------------------------------------------------
#include "everloop.h"
#include "everloop_image.h"
#include "voice_memory_map.h"
#include "microphone_array.h"
#include "wishbone_bus.h"

// ---------------------------------------------------------------------------
// DEFINES
// ---------------------------------------------------------------------------
//To configure these values, run 'make menuconfig'.
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASS
#define MQTT_HOST CONFIG_MQTT_HOST
#define MQTT_PORT CONFIG_MQTT_PORT
#define MQTT_USER CONFIG_MQTT_USER
#define MQTT_PASS CONFIG_MQTT_PASS
#define RATE 16000
#define CHUNK 256 //set to multiplications of 256, voice return a set of 256
#define WIDTH 2
#define CHANNELS 1
#define TOPIC "hermes/audioServer/livingroom/audioFrame"
#define BUFFER_SIZE 1000 //be sure to create enough buffer

bool connected = false;
bool send_wave = true; //send wave files instead of raw audio

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

namespace hal = matrix_hal;
hal::MicrophoneArray mics;

// ---------------------------------------------------------------------------
// FUNCTIONS copied from the esp32-mqtt example
// ---------------------------------------------------------------------------

/* Initialize non-volatile storage. If there are no pages free, erase the
 * contents of the flash memory, and attempt to initialize the storage
 * again. */
void initialize_nvs(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK( ret );
}

/* Initialize the TCP/IP stack and set the Wi-Fi default configuration and
 * operating mode. */
void
initialize_wifi(void)
{
    // Disable wifi driver logging
    esp_log_level_set("wifi", ESP_LOG_NONE);

    tcpip_adapter_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

/* Connect to the wireless network defined via menuconfig, using the supplied
 * passphrase. */
void
wifi_connect(void)
{
    wifi_config_t cfg = {};
    strcpy((char*)cfg.sta.ssid, WIFI_SSID);
    strcpy((char*)cfg.sta.password, WIFI_PASS);
    
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}

/* Main event loop handler.
 * On system start, attempt to connect to the configured wireless network.
 * If an IP address is acquired, set the 'Connected' bit for the Event Group,
 * and start the MQTT client.
 * On disconnect, stop the MQTT client and reset the 'Connected' bit. */
static
esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        esp_mqtt_start(MQTT_HOST, MQTT_PORT, "esp-mqtt", MQTT_USER, MQTT_PASS);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_mqtt_stop();
        esp_wifi_connect();
        break;
    default:
        break;
    }

    return ESP_OK;
}

/* The MQTT Status callback function.
 * If the status is 'Connected', subscribe to the defined MQTT channel, and
 * create a task to run the 'process' function implemented above. Enable the
 * LED as well.
 * On disconnect, destroy the task and disable the LED. */
static void
mqtt_status_cb(esp_mqtt_status_t status)
{
    switch (status)
    {
    case ESP_MQTT_STATUS_CONNECTED:
        //set connected to true to be able to send packets
        connected = true;
        break;
    case ESP_MQTT_STATUS_DISCONNECTED:
        //Not connected anymore, stop trying to send
        connected = false;
        break;
    }
}

/* The MQTT message callback function. When a message is received, print the
 * topic, payload, and the length of the payload to the terminal. */
static void
mqtt_message_cb(const char *topic, uint8_t *payload, size_t len)
{
    //TODO: Create stuff to control the leds on the voice for visual effects
    printf("incoming\t%s:%s (%d)\n", topic, payload, (int)len);
}

// ---------------------------------------------------------------------------
// MAIN LOOP
// ---------------------------------------------------------------------------
void cpp_loop(void)
{ 
    initialize_nvs();

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    initialize_wifi();
    
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

    esp_mqtt_init(mqtt_status_cb, mqtt_message_cb, BUFFER_SIZE, 2000);
    
    // ---------------------------------------------------------------------------
    // MATRIX VOICE STUFF
    // ---------------------------------------------------------------------------
    hal::WishboneBus wb;
    wb.Init();

    //setup everloop
    hal::Everloop everloop;
    hal::EverloopImage image1d;
    everloop.Setup(&wb);
    
    //setup mics
    mics.Setup(&wb);
    mics.SetSamplingRate(RATE);
    mics.ReadConfValues();
    //Hmm, is this actually needed and what does it do?
    mics.CalculateDelays(0, 0, 1000, 320 * 1000);  
    
    uint16_t voicebuffer[CHUNK];
    uint8_t voicemapped[CHUNK*WIDTH];
    
    while (true) {
      if (connected) {
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
        
        if (send_wave) {
            uint8_t payload[sizeof(header)+(CHUNK*WIDTH)];
            //Add the wave header
            memcpy(payload,&header,sizeof(header));
            memcpy(&payload[sizeof(header)], voicemapped, sizeof(voicemapped));
        
            //Ok, we can now  send the wave message
            esp_mqtt_publish(TOPIC, (uint8_t *)payload, sizeof(payload),0, false);
        } else {
            //Send the raw audio
            esp_mqtt_publish(TOPIC, (uint8_t *)voicemapped, sizeof(voicemapped),0, false);
        }
      }
    }
}

extern "C" {
   void app_main(void) {cpp_loop();}
}