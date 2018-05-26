/* ************************************************************************* *
 * Matrix Voice Audio Streamer - No OTA
 * 
 * This program is written to be a streaming audio server running on the Matrix Voice.
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
#include "everloop.h"
#include "everloop_image.h"
#include "voice_memory_map.h"
#include "microphone_array.h"
#include "wishbone_bus.h"
#include "freertos/event_groups.h"

#define SSID CONFIG_WIFI_SSID
#define PASSWORD CONFIG_WIFI_PASS
#define RATE 16000
#define CHUNK 256 //set to multiplications of 256, voice return a set of 256
#define WIDTH 2
#define CHANNELS 1
#define AP_CONNECTION_ESTABLISHED (1 << 0)

namespace hal = matrix_hal;
hal::MicrophoneArray mics;
hal::Everloop everloop;
hal::EverloopImage image1d;
static EventGroupHandle_t sEventGroup;

int networkIsConnected()
{
    return xEventGroupGetBits(sEventGroup) & AP_CONNECTION_ESTABLISHED;
}

static void networkSetConnected(uint8_t c)
{
    if (c) {
        xEventGroupSetBits(sEventGroup, AP_CONNECTION_ESTABLISHED);
    } else {
        xEventGroupClearBits(sEventGroup, AP_CONNECTION_ESTABLISHED);
    }
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_GOT_IP:
            networkSetConnected(1);
            break;
            
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            networkSetConnected(0);
            break;
            
        default:
            break;
    }
    
  return ESP_OK;
}

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
    nvs_flash_init();
    sEventGroup = xEventGroupCreate();
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
    
    //connect
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );
 
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
        if (networkIsConnected()) {
            //connected
            setEverloop(0,0,10,0);
        }    
    }
   
}

extern "C" {
   void app_main(void) {cpp_loop();}
}