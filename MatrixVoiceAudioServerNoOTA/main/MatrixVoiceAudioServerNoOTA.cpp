/* ************************************************************************* *
 * Matrix Voice Audio Streamer - No OTA
 * 
 * This program is written to be a streaming audio server running on the Matrix Voice.
 * It does not have an OTA network task, only the MQTT task.
 * Some errors were reported with this OTA task, so this version is created
 *  
 * Author:  Paul Romkes
 * Date:    May 2018
 * Version: 1
 * 
 * Changelog:
 * ==========
 * v1:
 *  - first code release.
 * ************************************************************************ */

#include "string.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "cJSON.h"
#include "everloop.h"
#include "everloop_image.h"
#include "voice_memory_map.h"
#include "microphone_array.h"
#include "wishbone_bus.h"
#include "freertos/event_groups.h"
extern "C" {
    #include "esp_mqtt.h"
}

#define SSID CONFIG_WIFI_SSID
#define PASSWORD CONFIG_WIFI_PASS
#define SITEID CONFIG_SITEID
#define MQTT_HOST CONFIG_MQTT_HOST
#define MQTT_PORT CONFIG_MQTT_PORT
#define MQTT_USER CONFIG_MQTT_USER
#define MQTT_PASS CONFIG_MQTT_PASS
#define RATE 16000
#define CHUNK 256 //set to multiplications of 256, voice return a set of 256
#define WIDTH 2
#define CHANNELS 1
#define BUFFER_SIZE 1000 //be sure to create enough buffer
#define AP_CONNECTION_ESTABLISHED (1 << 0)
#define MQTT_CONNECTION_ESTABLISHED (1 << 0)
#define HOTWORD_DETECTED (1 << 0)

namespace hal = matrix_hal;
hal::MicrophoneArray mics;
hal::Everloop everloop;
hal::EverloopImage image1d;
static EventGroupHandle_t sEventGroup;
static EventGroupHandle_t mqttEventGroup;
static EventGroupHandle_t hwEventGroup;
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

// ---------------------------------------------------------------------------
// EVERLOOP (The Led Ring)
// ---------------------------------------------------------------------------
void setEverloop(int red, int green, int blue, int white) {
    for (hal::LedValue& led : image1d.leds) {
      led.red = red;
      led.green = green;
      led.blue = blue;
      led.white = white;
    }

    everloop.Write(&image1d);
}

// ---------------------------------------------------------------------------
// FUNCTIONS FOR SEVERAL STATUSCHECKS
// ---------------------------------------------------------------------------
int mqttIsConnected()
{
    return xEventGroupGetBits(mqttEventGroup) & MQTT_CONNECTION_ESTABLISHED;
}

int HotwordDetected()
{
    return xEventGroupGetBits(hwEventGroup) & HOTWORD_DETECTED;
}

int networkIsConnected()
{
    return xEventGroupGetBits(sEventGroup) & AP_CONNECTION_ESTABLISHED;
}

// ---------------------------------------------------------------------------
// FUNCTIONS TO SET STATUSSES
// ---------------------------------------------------------------------------
static void networkSetConnected(uint8_t c)
{
    if (c) {
        xEventGroupSetBits(sEventGroup, AP_CONNECTION_ESTABLISHED);
    } else {
        xEventGroupClearBits(sEventGroup, AP_CONNECTION_ESTABLISHED);
    }
}

static void MqttSetConnected(uint8_t c)
{
    if (c) {
        xEventGroupSetBits(mqttEventGroup, MQTT_CONNECTION_ESTABLISHED);
    } else {
        xEventGroupClearBits(mqttEventGroup, MQTT_CONNECTION_ESTABLISHED);
    }
}

static void HotWordSetDetected(uint8_t c)
{
    if (c) {
        xEventGroupSetBits(hwEventGroup, HOTWORD_DETECTED);
    } else {
        xEventGroupClearBits(hwEventGroup, HOTWORD_DETECTED);
    }
}

// ---------------------------------------------------------------------------
// FUNCTIONS FOR MQTT HANDLING
// ---------------------------------------------------------------------------
static void mqtt_status_cb(esp_mqtt_status_t status) {
    switch (status)
    {
    case ESP_MQTT_STATUS_CONNECTED:
        //set connected to true to be able to send packets
        esp_mqtt_subscribe("hermes/hotword/toggleOff", 0);
        esp_mqtt_subscribe("hermes/hotword/toggleOn", 0);
        MqttSetConnected(1);
        break;
    case ESP_MQTT_STATUS_DISCONNECTED:
        //Not connected anymore, stop trying to send
        MqttSetConnected(0);
        //reconnect
        esp_mqtt_start(MQTT_HOST, MQTT_PORT, "esp-mqtt", MQTT_USER, MQTT_PASS);
        break;
    }
}

static void mqtt_message_cb(const char *topic, uint8_t *payload, size_t len) {
    const cJSON *site = NULL;
    if ((int)strstr (topic, "toggleOff") > 0) {
        cJSON *json = cJSON_Parse((const char *)payload);
        site = cJSON_GetObjectItemCaseSensitive(json, "siteId");
        if (strcmp(site->valuestring,SITEID) == 0) {
          HotWordSetDetected(1);
        }    
        cJSON_Delete(json);
    } else if ((int)strstr (topic, "toggleOn") > 0) {
        cJSON *json = cJSON_Parse((const char *)payload);
        site = cJSON_GetObjectItemCaseSensitive(json, "siteId");
        if (strcmp(site->valuestring,SITEID) == 0) {
         HotWordSetDetected(0);
        }
        cJSON_Delete(json);
    }
}

// ---------------------------------------------------------------------------
// FUNCTIONS FOR NETWORK HANDLING
// ---------------------------------------------------------------------------
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_GOT_IP:
            esp_mqtt_start(MQTT_HOST, MQTT_PORT, "esp-mqtt", MQTT_USER, MQTT_PASS);
            networkSetConnected(1);
            break;
            
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_mqtt_stop();
            esp_wifi_connect();
            networkSetConnected(0);
            break;
            
        default:
            break;
    }
    
  return ESP_OK;
}

// ---------------------------------------------------------------------------
// MAIN LOOP
// ---------------------------------------------------------------------------
int cpp_loop(void)
{
    // ---------------------------------------------------------------------------
    // CREATE WAVE HEADER
    // ---------------------------------------------------------------------------
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

    // ---------------------------------------------------------------------------
    // INIT ESP STUFF
    // ---------------------------------------------------------------------------
    nvs_flash_init();
    sEventGroup = xEventGroupCreate();
    mqttEventGroup = xEventGroupCreate();
    hwEventGroup = xEventGroupCreate();

    // ---------------------------------------------------------------------------
    // INIT MQTT
    // ---------------------------------------------------------------------------
    esp_mqtt_init(mqtt_status_cb, mqtt_message_cb, BUFFER_SIZE, 2000);

    // ---------------------------------------------------------------------------
    // CONNECT TO NETWORK
    // ---------------------------------------------------------------------------
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t sta_config = {};
    strcpy((char*)sta_config.sta.ssid, SSID);
    strcpy((char*)sta_config.sta.password, PASSWORD);
    sta_config.sta.bssid_set = false;
    
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );
     
    // ---------------------------------------------------------------------------
    // SETUP MATRIX VOICE
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
        if (networkIsConnected() && !HotwordDetected()) {
            //connected
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