/* ************************************************************************* *
 * Matrix Voice Audio Streamer
 * 
 * This program is written to be a streaming audio server running on the Matrix Voice.
 This is typically used for Snips.AI, it will then be able to replace
 * the Snips Audio Server, by publishing small wave messages to the hermes protocol
 * See https://snips.ai/ for more information
 * The audio server splits the stream in 2 parts, so that it will fit the default configuration of 256
 * 
 * Author:  Paul Romkes
 * Date:    September 2018
 * Version: 3.2
 * 
 * Changelog:
 * ==========
 * v1:
 *  - first code release. It needs a lot of improvement, no hardcoding stuff
 * v2:
 *  - Change to Arduino IDE
 * v2.1:
 *  - Changed to pubsubclient and fixed other stability issues
 * v3:
 *  - Add OTA
 * v3.1:
 *  - Only listen to SITEID to toggle hotword
 *  - Got rid of String, leads to Heap Fragmentation
 *  - Add dynamic brihtness, post {"brightness": 50 } to SITEID/everloop
 *  - Fix stability, using semaphores
 * v3.2:
 *  - Add dynamic colors, see readme for documentation
 *  - Restart the device by publishing hashed password to SITEID/restart
 * ************************************************************************ */
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <AsyncMqttClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <sstream>
#include <string>
#include "wishbone_bus.h"
#include "everloop.h"
#include "everloop_image.h"
#include "microphone_array.h"
#include "microphone_core.h"
#include "voice_memory_map.h"
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
  #include "freertos/event_groups.h"
}
#include "config.h"

//Some needed defines, should be obvious which ones to change
#define RATE 16000
#define SITEID "matrixvoice"
#define MQTT_IP IPAddress(192, 168, 178, 194)
#define MQTT_HOST "192.168.178.194"
#define MQTT_PORT 1883
#define CHUNK 256 //set to multiplications of 256, voice return a set of 256
#define WIDTH 2
#define CHANNELS 1

namespace hal = matrix_hal;
static hal::WishboneBus wb;
static hal::Everloop everloop;
static hal::MicrophoneArray mics;
static hal::EverloopImage image1d;
WiFiClient net;
AsyncMqttClient asyncClient; //ASYNCH client to be able to handle huge messages like WAV files
PubSubClient audioServer(net); //We also need a sync client, asynch leads to errors on the audio thread
//Timers
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
SemaphoreHandle_t wbSemaphore;

//Globals
struct wavfile_header {
  char  riff_tag[4];    //4
  int   riff_length;    //4
  char  wave_tag[4];    //4
  char  fmt_tag[4];     //4
  int   fmt_length;     //4
  short audio_format;   //2
  short num_channels;   //2
  int   sample_rate;    //4
  int   byte_rate;      //4
  short block_align;    //2
  short bits_per_sample;//2
  char  data_tag[4];    //4
  int   data_length;    //4
};

static struct wavfile_header header;
const int EVERLOOP = BIT0; //Used to check if the everloop should be updated
static EventGroupHandle_t everloopGroup;
//Sound buffers
static uint16_t voicebuffer[CHUNK];
static uint8_t voicemapped[CHUNK*WIDTH];
static uint8_t payload[sizeof(header)+(CHUNK*WIDTH)];
static uint8_t payload2[sizeof(header)+(CHUNK*WIDTH)];

//Colors. TODO: implement code like bightnesss for color changing without coding
int hotword_colors[4] = {0,255,0,0};
int idle_colors[4] = {0,0,255,0};
int disconnect_colors[4] = {255,0,0,0};
int update_colors[4] = {0,0,0,255};
int brightness = 50;
bool DEBUG = false; //If set to true, code will post several messages to topics in case of events
bool boot = true;
bool disconnected = false;
bool hotword_detected = false;
bool update = false;
//Change to your own password hash at https://www.md5hashgenerator.com/
const char* passwordhash = "4b8d34978fafde81a85a1b91a09a881b";

//Dynamic topics for MQTT
std::string audioFrameTopic = std::string("hermes/audioServer/") + SITEID + std::string("/audioFrame");
std::string playFinishedTopic = std::string("hermes/audioServer/") + SITEID + std::string("/playFinished");
std::string playBytesTopic = std::string("hermes/audioServer/") + SITEID + std::string("/playBytes/#");
std::string toggleOffTopic = "hermes/hotword/toggleOff";
std::string toggleOnTopic = "hermes/hotword/toggleOn";
std::string everloopTopic = SITEID + std::string("/everloop");
std::string debugTopic = "debug/asynch/status";
std::string debugAudioTopic = "debug/audioserver/status";
std::string restartTopic = SITEID + std::string("/restart");

//This is used to be able to change brightness, while keeping the colors appear the same
//Called gamma correction, check this https://learn.adafruit.com/led-tricks-gamma-correction/the-issue
const uint8_t PROGMEM gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

// ---------------------------------------------------------------------------
// EVERLOOP, Only use this function in SETUP
// When using it in other parts, it might crash the ESP because of the SPI
// being also used by the audiostream. A mutex task has been implemented for that
// ---------------------------------------------------------------------------
void setEverloop(int red, int green, int blue, int white) {
    int r = pgm_read_byte(&gamma8[(int)round(brightness * red / 100)]);
    int g = pgm_read_byte(&gamma8[(int)floor(brightness * green / 100)]);
    int b = pgm_read_byte(&gamma8[(int)floor(brightness * blue / 100)]);
    int w = pgm_read_byte(&gamma8[(int)floor(brightness * white / 100)]);
    for (hal::LedValue& led : image1d.leds) {
      led.red = r;
      led.green = g;
      led.blue = b;
      led.white = w;
    }
    everloop.Write(&image1d);
}

// ---------------------------------------------------------------------------
// Network functions
// ---------------------------------------------------------------------------
void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(SSID, PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to asynch MQTT...");
  asyncClient.connect();
}

void connectAudio() {
 Serial.println("Connecting to synch MQTT...");
 audioServer.connect("MatrixVoiceAudio");
}

// ---------------------------------------------------------------------------
// WIFI event
// Kicks off various stuff in case of connect/disconnect
// ---------------------------------------------------------------------------
void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
  case SYSTEM_EVENT_STA_GOT_IP:
    xEventGroupSetBits(everloopGroup, EVERLOOP); //Set the bit so the everloop gets updated
    connectToMqtt();
    connectAudio();
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    xTimerStop(mqttReconnectTimer, 0); //Do not reconnect to MQTT while reconnecting to network
    xTimerStart(wifiReconnectTimer, 0); //Start the reconnect timer
    break;
  default:
    break;
  }
}

// ---------------------------------------------------------------------------
// MQTT Connect event
// ---------------------------------------------------------------------------
void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  asyncClient.subscribe(playBytesTopic.c_str(), 0);
  asyncClient.subscribe(toggleOffTopic.c_str(),0);
  asyncClient.subscribe(toggleOnTopic.c_str(),0);
  asyncClient.subscribe(everloopTopic.c_str(),0);
  asyncClient.subscribe(restartTopic.c_str(),0);

  if (DEBUG) {
    if (boot) {
      boot = false;
      asyncClient.publish(debugTopic.c_str(),0, false, "boot");
    }
    if (disconnected) {
      asyncClient.publish(debugTopic.c_str(),0, false, "reconnect");
      disconnected = false;
    }
  }
}

// ---------------------------------------------------------------------------
// MQTT Disonnect event
// ---------------------------------------------------------------------------
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  disconnected = true;
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

// ---------------------------------------------------------------------------
// MQTT Callback
// Sets the HOTWORD bits to toggle the leds and published a message on playFinished
// to simulate the playFinished. Without it, Snips will not start listening when the 
// feedback sound is toggled on
// ---------------------------------------------------------------------------
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  std::string topicstr(topic);
  if (len + index >= total) {
    if (topicstr.find("toggleOff") != std::string::npos) {
      std::string payloadstr(payload);
      //Check if this is for us
      if (payloadstr.find(SITEID) != std::string::npos) {
        hotword_detected = true;
        xEventGroupSetBits(everloopGroup, EVERLOOP); //Set the bit so the everloop gets updated
      }
    } else if (topicstr.find("toggleOn") != std::string::npos) {
      //Check if this is for us
      std::string payloadstr(payload);
      if (payloadstr.find(SITEID) != std::string::npos) {
        hotword_detected = false;
        xEventGroupSetBits(everloopGroup, EVERLOOP); //Set the bit so the everloop gets updated
      }
    } else if (topicstr.find("playBytes") != std::string::npos) {
      int pos = 19 + strlen(SITEID) + 11;
      std::string s  = "{\"id\":\"" + topicstr.substr(pos,37) + "\",\"siteId\":\"" + SITEID + "\",\"sessionId\":null}";
      if (asyncClient.connected()) {
        asyncClient.publish(playFinishedTopic.c_str(), 0, false, s.c_str());
      }
    } else if (topicstr.find(everloopTopic.c_str()) != std::string::npos) {
      StaticJsonBuffer<300> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject((char *) payload);
      if (root.success()) {
        if (root.containsKey("brightness")) {
          brightness = root["brightness"];          
        }
        if (root.containsKey("hotword")) {
          hotword_colors[0] = root["hotword"][0];          
          hotword_colors[1] = root["hotword"][1];          
          hotword_colors[2] = root["hotword"][2];          
          hotword_colors[3] = root["hotword"][3];          
        }
        if (root.containsKey("idle")) {
          idle_colors[0] = root["idle"][0];          
          idle_colors[1] = root["idle"][1];          
          idle_colors[2] = root["idle"][2];          
          idle_colors[3] = root["idle"][3];          
        }
        if (root.containsKey("disconnect")) {
          disconnect_colors[0] = root["disconnect"][0];          
          disconnect_colors[1] = root["disconnect"][1];          
          disconnect_colors[2] = root["disconnect"][2];          
          disconnect_colors[3] = root["disconnect"][3];          
        }
        if (root.containsKey("update")) {
          update_colors[0] = root["update"][0];          
          update_colors[1] = root["update"][1];          
          update_colors[2] = root["update"][2];          
          update_colors[3] = root["update"][3];          
        }
        xEventGroupSetBits(everloopGroup, EVERLOOP);
      }
    } else if (topicstr.find(restartTopic.c_str()) != std::string::npos) {
      StaticJsonBuffer<300> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject((char *) payload);
      if (root.success()) {
        if (root.containsKey("passwordhash")) {
          if (root["passwordhash"] == passwordhash) {
            ESP.restart();          
          }
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Audiostream -  uses SYNC MQTT client
// ---------------------------------------------------------------------------
void Audiostream( void * parameter ) {
   for(;;){
      // See if we can obtain or "Take" the Serial Semaphore.
      // If the semaphore is not available, wait 5 ticks of the Scheduler to see if it becomes free.
    if (audioServer.connected() && (xSemaphoreTake( wbSemaphore, ( TickType_t ) 5 ) == pdTRUE) ) {
      //We are connected! 
      mics.Read();
      
      //NumberOfSamples() = kMicarrayBufferSize / kMicrophoneChannels = 4069 / 8 = 512
      for (uint32_t s = 0; s < CHUNK; s++) {
          voicebuffer[s] = mics.Beam(s);
      }

      //voicebuffer will hold 256 samples of 2 bytes, but we need it as 1 byte
      //We do a memcpy, because I need to add the wave header as well
      memcpy(voicemapped,voicebuffer,CHUNK*WIDTH);
  
      //Add the wave header
      memcpy(payload,&header,sizeof(header));
      memcpy(&payload[sizeof(header)], voicemapped, sizeof(voicemapped));

      audioServer.publish(audioFrameTopic.c_str(), (uint8_t *)payload, sizeof(payload));
      
      //also send to second half as a wav
      for (uint32_t s = CHUNK; s < CHUNK*WIDTH; s++) {
          voicebuffer[s-CHUNK] = mics.Beam(s);
      }
  
      memcpy(voicemapped,voicebuffer,CHUNK*WIDTH);
  
      //Add the wave header
      memcpy(payload2,&header,sizeof(header));
      memcpy(&payload2[sizeof(header)], voicemapped, sizeof(voicemapped));
      
      audioServer.publish(audioFrameTopic.c_str(), (uint8_t *)payload2, sizeof(payload2));
      xSemaphoreGive( wbSemaphore ); // Now free or "Give" the Serial Port for others.
    }
    vTaskDelay(1);
  }
  vTaskDelete(NULL);
}

void everloopTask(void *pvParam){
    while(1){
      xEventGroupWaitBits(everloopGroup,EVERLOOP,true,true,portMAX_DELAY); //Wait for the bit before updating
      Serial.println("Updating everloop");
      //Implementation of Semaphore, otherwise the ESP will crash due to read of the mics
      if ( xSemaphoreTake( wbSemaphore, ( TickType_t ) 100 ) == pdTRUE )
      {
        //Yeah got it, see what colors whe need
        int r = 0;
        int g = 0;
        int b = 0;
        int w = 0;
        if (update) {
          r = update_colors[0];
          g = update_colors[1];
          b = update_colors[2];
          w = update_colors[3];
        } else if (hotword_detected) {
          r = hotword_colors[0];
          g = hotword_colors[1];
          b = hotword_colors[2];
          w = hotword_colors[3];
        } else {
          r = idle_colors[0];
          g = idle_colors[1];
          b = idle_colors[2];
          w = idle_colors[3];
        }
        r = floor(brightness * r / 100);
        r = pgm_read_byte(&gamma8[r]);
        g = floor(brightness * g / 100);
        g = pgm_read_byte(&gamma8[g]);
        b = floor(brightness * b / 100);
        b = pgm_read_byte(&gamma8[b]);
        w = floor(brightness * w / 100);
        w = pgm_read_byte(&gamma8[w]);
        for (hal::LedValue& led : image1d.leds) {
          led.red = r;
          led.green = g;
          led.blue = b;
          led.white = w;
        }
        everloop.Write(&image1d);
        xSemaphoreGive( wbSemaphore ); //Free for all
        xEventGroupClearBits(everloopGroup,EVERLOOP); //Clear the everloop bit
        Serial.println("Updating done");
      }
      vTaskDelay(1); //Delay a tick, for better stability 
    }
    vTaskDelete(NULL);
}

void setup() {  
  //Implementation of Semaphore, otherwise the ESP will crash due to read of the mics
  if ( wbSemaphore == NULL )  //Not yet been created?
  {
    wbSemaphore = xSemaphoreCreateMutex();  //Create a mutex semaphore
    if ( ( wbSemaphore ) != NULL )
      xSemaphoreGive( ( wbSemaphore ) ); //Free for all 
  }
    
  //Reconnect timers
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
 
  WiFi.onEvent(WiFiEvent);

  asyncClient.onConnect(onMqttConnect);
  asyncClient.onDisconnect(onMqttDisconnect);
  asyncClient.onMessage(onMqttMessage);
  asyncClient.setServer(MQTT_IP, MQTT_PORT);
  audioServer.setServer(MQTT_IP, 1883);  

  everloopGroup = xEventGroupCreate();
 
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
  
  wb.Init();
  everloop.Setup(&wb);
  
  //setup mics
  mics.Setup(&wb);
  mics.SetSamplingRate(RATE);
  //mics.SetGain(5);

   // Microphone Core Init
  hal::MicrophoneCore mic_core(mics);
  mic_core.Setup(&wb);

  //Use function in setup
  setEverloop(disconnect_colors[0],disconnect_colors[1],disconnect_colors[2],disconnect_colors[3]);
  
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  //Use function in setup
  setEverloop(idle_colors[0],idle_colors[1],idle_colors[2],idle_colors[3]);

  //Create the runnings tasks, AudioStream is on 1 core, the rest on the other core
  xTaskCreatePinnedToCore(Audiostream,"Audiostream",10000,NULL,1,NULL,0);
  xTaskCreatePinnedToCore(everloopTask,"everloopTask",4096,NULL,3,NULL,1);
    
  // ---------------------------------------------------------------------------
  // ArduinoOTA stuff
  // ---------------------------------------------------------------------------
  ArduinoOTA.setHostname("MatrixVoice");  
  ArduinoOTA.setPasswordHash(passwordhash);
  ArduinoOTA
    .onStart([]() {
      update = true;
      xEventGroupSetBits(everloopGroup, EVERLOOP);
    })
    .onEnd([]() {
      update = false;
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  
  if (!audioServer.connected()) {
    if (asyncClient.connected() && DEBUG) {
      asyncClient.publish(debugAudioTopic.c_str(),0, false, "reconnect");
    }
    connectAudio();
  }
}
