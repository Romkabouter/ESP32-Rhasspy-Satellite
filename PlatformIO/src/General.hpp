#include <tinyfsm.hpp>
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <PubSubClient.h>
#include "RingBuf.h"
#include "SPIFFS.h"
#include "ESPAsyncWebServer.h"
#include <ArduinoJson.h>
#include "index_html.h"

const int PLAY = BIT0;
const int STREAM = BIT1;

enum {
  HW_LOCAL = 0,
  HW_REMOTE = 1
};

AsyncWebServer server(80);
//Configuration defaults
struct Config {
  IPAddress mqtt_host = MQTT_IP;
  bool mqtt_valid = true;
  int mqtt_port = MQTT_PORT;
  std::string mqtt_user = MQTT_USER;
  std::string mqtt_pass = MQTT_PASS;
  bool mute_input = false;
  bool mute_output = false;
  uint16_t hotword_detection = HW_REMOTE;
  uint16_t amp_output = AMP_OUT_HEADPHONE;
  int brightness = 15;
  int hotword_brightness = 15;  
  uint16_t volume = 100;
  int gain = 5;
};
const char *configfile = "/config.json"; 
Config config;

struct wavfile_header {
    char riff_tag[4];       // 4
    int riff_length;        // 4
    char wave_tag[4];       // 4
    char fmt_tag[4];        // 4
    int fmt_length;         // 4
    short audio_format;     // 2
    short num_channels;     // 2
    int sample_rate;        // 4
    int byte_rate;          // 4
    short block_align;      // 2
    short bits_per_sample;  // 2
    char data_tag[4];       // 4
    int data_length;        // 4
};
struct wavfile_header header;
std::string finishedMsg = "";
bool mqttInitialized = false;
int retryCount = 0;
int I2SMode = -1;
bool mqttConnected = false;
bool DEBUG = false;
std::string audioFrameTopic = std::string("hermes/audioServer/") + SITEID + std::string("/audioFrame");
std::string playBytesTopic = std::string("hermes/audioServer/") + SITEID + std::string("/playBytes/#");
std::string playFinishedTopic = std::string("hermes/audioServer/") + SITEID + std::string("/playFinished");
std::string hotwordTopic = "hermes/hotword/#";
std::string audioTopic = SITEID + std::string("/audio");
std::string ledTopic = SITEID + std::string("/led");
std::string debugTopic = SITEID + std::string("/debug");
std::string restartTopic = SITEID + std::string("/restart");
AsyncMqttClient asyncClient; 
WiFiClient net;
PubSubClient audioServer(net); 
RingBuf<uint8_t, 60000> audioData;
long message_size = 0;
int queueDelay = 10;
int sampleRate = 16000;
int numChannels = 2;
int bitDepth = 16;
static EventGroupHandle_t audioGroup;
SemaphoreHandle_t wbSemaphore;

struct WifiDisconnected;
struct MQTTDisconnected;
struct HotwordDetected;
struct Idle;
struct PlayAudio;

struct WifiDisconnectEvent : tinyfsm::Event { };
struct WifiConnectEvent : tinyfsm::Event { };
struct MQTTDisconnectedEvent : tinyfsm::Event { };
struct MQTTConnectedEvent : tinyfsm::Event { };
struct IdleEvent : tinyfsm::Event { };
struct StreamAudioEvent : tinyfsm::Event { };
struct PlayAudioEvent : tinyfsm::Event {};
struct HotwordDetectedEvent : tinyfsm::Event { };

void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void publishDebug(const char* message);
void InitI2SSpeakerOrMic(int mode);
void WiFiEvent(WiFiEvent_t event);
void initHeader(int readSize, int width, int rate);
void MQTTtask(void *p);
void I2Stask(void *p);
void loadConfiguration(const char *filename, Config &config);
void saveConfiguration(const char *filename, Config &config);

/* ************************************************************************* *
      HELPER CLASS FOR WAVE HEADER, taken from https://www.xtronical.com/
      Changed to fit my needs
 * ************************************************************************ */
// Convert 4 byte little-endian to a long,
#define longword(bfr, ofs) (bfr[ofs + 3] << 24 | bfr[ofs + 2] << 16 | bfr[ofs + 1] << 8 | bfr[ofs + 0])
#define shortword(bfr, ofs) (bfr[ofs + 1] << 8 | bfr[ofs + 0])
#define DATA_CHUNK_ID 0x61746164
#define FMT_CHUNK_ID 0x20746d66

class XT_Wav_Class {
   public:
    uint16_t NumChannels;
    uint16_t SampleRate;
    uint32_t DataStart;      // offset of the actual data.
    uint16_t Format;         // WAVE Format Code
    uint16_t BitsPerSample;  // WAVE bits per sample
    // constructors
    XT_Wav_Class(const unsigned char *WavData);
};

XT_Wav_Class::XT_Wav_Class(const unsigned char *WavData) {
    unsigned long ofs;
    ofs = 12;
    SampleRate = DataStart = BitsPerSample = Format = NumChannels = 0;
    while (ofs < 44) {
        if (longword(WavData, ofs) == DATA_CHUNK_ID) {
            DataStart = ofs + 8;
        }
        if (longword(WavData, ofs) == FMT_CHUNK_ID) {
            Format = shortword(WavData, ofs + 8);
            NumChannels = shortword(WavData, ofs + 10);
            SampleRate = longword(WavData, ofs + 12);
            BitsPerSample = shortword(WavData, ofs + 22);
        }
        ofs += longword(WavData, ofs + 4) + 8;
    }
}

uint8_t WaveData[44];

String processor(const String& var){
  if(var == "MQTT_HOST"){
      return config.mqtt_host.toString();
  }
  if(var == "MQTT_PORT"){
      return String(config.mqtt_port);
  }
  if(var == "MQTT_USER"){
      return String(config.mqtt_user.c_str());
  }
  if(var == "MQTT_PASS"){
      return String(config.mqtt_pass.c_str());
  }
  if (var == "MUTE_INPUT") {
      return (config.mute_input) ? "checked" : "";
  }
  if (var == "MUTE_OUTPUT") {
      return (config.mute_output) ? "checked" : "";
  }
  if (var == "AMP_OUT_SPEAKERS") {
      return (config.amp_output == AMP_OUT_SPEAKERS) ? "selected" : "";
  }
  if (var == "AMP_OUT_HEADPHONE") {
      return (config.amp_output == AMP_OUT_HEADPHONE) ? "selected" : "";
  }
  if (var == "BRIGHTNESS") {
      return String(config.brightness);
  }
  if (var == "HW_BRIGHTNESS") {
      return String(config.hotword_brightness);
  }
  if (var == "HW_LOCAL") {
      return (config.hotword_detection == HW_LOCAL) ? "selected" : "";
  }
  if (var == "HW_REMOTE") {
      return (config.hotword_detection == HW_REMOTE) ? "selected" : "";
  }
  if (var == "VOLUME") {
      return String(config.volume);
  }
  if (var == "GAIN") {
      return String(config.gain);
  }
  return String();
}

void handleFSf ( AsyncWebServerRequest* request, const String& route ) {
    AsyncWebServerResponse *response ;

    if ( route.indexOf ( "index.html" ) >= 0 ) // Index page is in PROGMEM
    {
        if (request->method() == HTTP_POST) {
            int params = request->params();
            bool saveNeeded = false;
            bool mi_found = false;
            bool mo_found = false;
            for(int i=0;i<params;i++){
                AsyncWebParameter* p = request->getParam(i);
                Serial.printf("Parameter %s, value %s\r\n", p->name().c_str(), p->value().c_str());
                if(p->name() == "mqtt_host"){
                    char ip[64];
                    strlcpy(ip,p->value().c_str(),sizeof(ip));
                    IPAddress adr;
                    adr.fromString(ip);
                    if (config.mqtt_host != adr) {
                    Serial.println("Mqtt host changed");
                    config.mqtt_valid = config.mqtt_host.fromString(ip);
                    saveNeeded = true;
                    }
                }
                if(p->name() == "mqtt_port"){
                    if (config.mqtt_port != (int)p->value().toInt()) {
                        Serial.println("Mqtt port changed");
                        config.mqtt_port = (int)p->value().toInt(); 
                        saveNeeded = true;
                    }
                }
                if(p->name() == "mqtt_user"){
                    if (config.mqtt_user != std::string(p->value().c_str())) {
                        Serial.println("Mqtt user changed");
                        config.mqtt_user = std::string(p->value().c_str());
                        saveNeeded = true;
                    }
                }
                if(p->name() == "mqtt_pass"){
                    if (config.mqtt_pass != std::string(p->value().c_str())) {
                        Serial.println("Mqtt password changed");
                        config.mqtt_pass = std::string(p->value().c_str()); 
                        saveNeeded = true;
                    }
                }
                if(p->name() == "mute_input"){
                    mi_found = true;
                    if (!config.mute_input && p->value().equals("on")) {
                        Serial.println("Mute input changed");
                        config.mute_input = true;   
                        saveNeeded = true;
                    }
                }
                if(p->name() == "mute_output"){
                    mo_found = true;
                    if (!config.mute_output && p->value().equals("on")) {
                        Serial.println("Mute output changed");
                        config.mute_output = true;       
                        saveNeeded = true;
                    }
                }
                if(p->name() == "amp_output"){
                    if (config.amp_output != (int)p->value().toInt()) {
                        Serial.println("Amp output changed");
                        config.amp_output = (p->value().equals("0")) ? AMP_OUT_SPEAKERS : AMP_OUT_HEADPHONE;       
                        saveNeeded = true;
                    }
                }
                if(p->name() == "brightness"){
                    if (config.brightness != (int)p->value().toInt()) {
                        Serial.println("Brightness changed");
                        config.brightness = (int)p->value().toInt();      
                        saveNeeded = true;
                    }
                }
                if(p->name() == "hw_brightness"){
                    if (config.hotword_brightness != (int)p->value().toInt()) {
                        Serial.println("Hotword brightness changed");
                        config.hotword_brightness = (int)p->value().toInt();
                        saveNeeded = true;
                    }
                }
                if(p->name() == "hotword_detection"){
                    if (config.hotword_detection != (int)p->value().toInt()) {
                        Serial.println("Hotword detection changed");
                        config.hotword_detection = (p->value().equals("0")) ? HW_LOCAL : HW_REMOTE;       
                        saveNeeded = true;
                    }
                }
                if(p->name() == "gain"){
                    if (config.gain != (int)p->value().toInt()) {
                        Serial.println("Gain changed");
                        config.gain = (int)p->value().toInt();      
                        saveNeeded = true;
                    }
                }
                if(p->name() == "volume"){
                    if (config.volume != (int)p->value().toInt()) {
                        Serial.println("Volume changed");
                        config.volume = (int)p->value().toInt();      
                        saveNeeded = true;
                    }
                }
            }
            if (!mi_found && config.mute_input) {
                Serial.println("Mute input not found, value = off");
                config.mute_input = false;
                saveNeeded = true;
            }
            if (!mo_found && config.mute_output) {
                Serial.println("Mute output not found, value = off");
                config.mute_output = false;
                saveNeeded = true;
            }
            if (saveNeeded) {
                Serial.println("Settings changed, saving configuration");
                saveConfiguration(configfile, config);
            } else {
                Serial.println("No settings changed");
            }
        }
        response = request->beginResponse_P ( 200, "text/html", index_html, processor ) ;
    } else {
        response = request->beginResponse_P ( 200, "text/html", "<html><body>unkown route</body></html>") ;
    }

    request->send ( response ) ;
}

void handleRequest ( AsyncWebServerRequest* request )
{
    handleFSf ( request, String( "/index.html") ) ;
}

void publishDebug(const char* message) {
    if (DEBUG) {
        asyncClient.publish(debugTopic.c_str(), 0, false, message);
    }
}

void loadConfiguration(const char *filename, Config &config) {
  File file = SPIFFS.open(filename);
  StaticJsonDocument<512> doc;
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration"));
  } else {
    serializeJsonPretty(doc, Serial);  
    config.mqtt_valid = config.mqtt_host.fromString(doc.getMember("mqtt_host").as<const char*>());
    config.mqtt_port = doc.getMember("mqtt_port").as<int>();
    config.mqtt_user = doc.getMember("mqtt_user").as<std::string>();
    config.mqtt_pass = doc.getMember("mqtt_pass").as<std::string>();
    config.mute_input = doc.getMember("mute_input").as<int>();
    config.mute_output = doc.getMember("mute_output").as<int>();
    //device->muteOutput(config.mute_output);
    config.amp_output = doc.getMember("amp_output").as<int>();
    //device->ampOutput(config.amp_output);
    config.brightness = doc.getMember("brightness").as<int>();
    //device->updateBrightness(config.brightness);
    config.hotword_brightness = doc.getMember("hotword_brightness").as<int>();
    config.hotword_detection = doc.getMember("hotword_detection").as<int>();
    config.volume = doc.getMember("volume").as<int>();
    //device->setVolume(config.volume);
    config.gain = doc.getMember("gain").as<int>();
    //device->setGain(config.gain);
    if (config.mqtt_host[0] == 0) {
        config.mqtt_valid = false;
    }
  }
  file.close();
}

void saveConfiguration(const char *filename, Config &config) {
    Serial.println("Saving configuration");
    if (SPIFFS.exists(filename)) {
        SPIFFS.remove(filename);
    }
    File file = SPIFFS.open(filename, "w");
    if (!file) {
        Serial.println(F("Failed to create file"));
        return;
    }
    StaticJsonDocument<256> doc;
    char ip[64];
    strlcpy(ip,config.mqtt_host.toString().c_str(),sizeof(ip)); 
    config.mqtt_valid = config.mqtt_host.fromString(ip);
    doc["mqtt_host"] = config.mqtt_host.toString();
    doc["mqtt_port"] = config.mqtt_port;
    doc["mqtt_user"] = config.mqtt_user;
    doc["mqtt_pass"] = config.mqtt_pass;
    doc["mqtt_valid"] = config.mqtt_valid;
    doc["mute_input"] = config.mute_input;
    doc["mute_output"] = config.mute_output;
    doc["amp_output"] = config.amp_output;
    doc["brightness"] = config.brightness;
    doc["hotword_brightness"] = config.hotword_brightness;
    doc["hotword_detection"] = config.hotword_detection;
    doc["volume"] = config.volume;
    doc["gain"] = config.gain;
    if (serializeJson(doc, file) == 0) {
         Serial.println(F("Failed to write to file"));
    }
    file.close();
    loadConfiguration(filename, config);
    audioServer.disconnect();
}