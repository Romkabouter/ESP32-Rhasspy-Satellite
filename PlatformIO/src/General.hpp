#pragma once

#include <tinyfsm.hpp>
#include <Arduino.h>

#if NETWORK_TYPE == NETWORK_ETHERNET
    #include <ETH.h>
#else
    #include <WiFi.h>
#endif

#include <AsyncMqttClient.h>
#include <PubSubClient.h>
#include "SPIFFS.h"
#include "ESPAsyncWebServer.h"
#include <ArduinoJson.h>
#include "index_html.h"
#include "Esp32RingBuffer.h"
#include <map>

const int PLAY = BIT0;
const int STREAM = BIT1;

enum {
  HW_LOCAL = 0,
  HW_REMOTE = 1
};

AsyncWebServer server(80);
//Configuration defaults
struct Config {
  std::string siteid = SITEID;
  std::string mqtt_host = MQTT_HOST;
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
  int animation = SOLID;
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
bool streamingBytes = false;
bool endStream = false;
bool mqttPaused = false;
bool configChanged = false;

std::string audioFrameTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/audioFrame");
std::string playBytesTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/playBytes/#");
std::string playBytesStreamingTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/playBytesStreaming/#");
std::string playFinishedTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/playFinished");
std::string streamFinishedTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/streamFinished");
std::string hotwordTopic = "hermes/hotword/#";
std::string audioTopic = config.siteid + std::string("/audio");
std::string ledTopic = config.siteid + std::string("/led");
std::string debugTopic = config.siteid + std::string("/debug");
std::string restartTopic = config.siteid + std::string("/restart");
std::string sayTopic = "hermes/tts/say";
std::string sayFinishedTopic = "hermes/tts/sayFinished";
std::string errorTopic = "hermes/nlu/intentNotRecognized";
AsyncMqttClient asyncClient; 
WiFiClient net;
PubSubClient audioServer(net); 
Esp32RingBuffer<uint8_t, uint16_t, (1U << 15)> audioData;
long message_size = 0;
int queueDelay = 10;
int sampleRate = 16000;
int numChannels = 2;
int bitDepth = 16;
int current_colors = COLORS_IDLE;
static EventGroupHandle_t audioGroup;
SemaphoreHandle_t wbSemaphore;
TaskHandle_t i2sHandle;

struct WifiConnected;
struct WifiDisconnected;
struct MQTTConnected;
struct MQTTDisconnected;
struct Listening;
struct ListeningPlay;
struct Idle;
struct IdlePlay;
struct Tts;
struct TtsPlay;
struct Error;
struct ErrorPlay;
struct Updating;

struct WifiDisconnectEvent : tinyfsm::Event { };
struct WifiConnectEvent : tinyfsm::Event { };
struct MQTTDisconnectedEvent : tinyfsm::Event { };
struct MQTTConnectedEvent : tinyfsm::Event { };
struct IdleEvent : tinyfsm::Event { };
struct TtsEvent : tinyfsm::Event { };
struct ErrorEvent : tinyfsm::Event { };
struct UpdateEvent : tinyfsm::Event { };
struct BeginPlayAudioEvent : tinyfsm::Event {};
struct EndPlayAudioEvent : tinyfsm::Event {};
struct StreamAudioEvent : tinyfsm::Event { };
struct PlayBytesEvent : tinyfsm::Event {};
struct ListeningEvent : tinyfsm::Event { };
struct UpdateConfigurationEvent : tinyfsm::Event { };

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


// to add more variables use a C++ lambda, it must either directly return a String, or specify return type ("-> String"), or both 
const std::map<const std::string, String (*)()> processor_values = {
    {"MQTT_HOST",           []() -> String { return config.mqtt_host.c_str();} },
    {"MQTT_PORT",           []() { return String(config.mqtt_port);} },
    {"MQTT_USER",           []() -> String { return config.mqtt_user.c_str(); } },
    {"MQTT_PASS",           []() -> String { return config.mqtt_pass.c_str(); } },
    {"MUTE_INPUT",          []() -> String { return (config.mute_input) ? "checked" : ""; } },
    {"MUTE_OUTPUT",         []() -> String { return(config.mute_output) ? "checked" : ""; } },
    {"AMP_OUT_SPEAKERS",    []() -> String { return device->numAmpOutConfigurations() < 1? "hidden" : (config.amp_output == AMP_OUT_SPEAKERS) ? "selected" : ""; } },
    {"AMP_OUT_HEADPHONE",   []() -> String { return device->numAmpOutConfigurations() < 2? "hidden" : (config.amp_output == AMP_OUT_HEADPHONE) ? "selected" : ""; } },
    {"AMP_OUT_BOTH",        []() -> String { return device->numAmpOutConfigurations() < 3? "hidden" : (config.amp_output == AMP_OUT_BOTH) ? "selected" : ""; } },
    {"BRIGHTNESS",          []() { return String(config.brightness); } },
    {"HW_BRIGHTNESS",       []() { return String(config.hotword_brightness); } },
    {"HW_LOCAL",            []() -> String { return (config.hotword_detection == HW_LOCAL) ? "selected" : ""; } },
    {"HW_REMOTE",           []() -> String { return (config.hotword_detection == HW_REMOTE) ? "selected" : ""; } },
    {"VOLUME",              []() { return String(config.volume); } },
    {"GAIN",                []() { return String(config.gain); } },
    {"SITEID",              []() -> String { return config.siteid.c_str(); } },
    {"ANIMATIONSUPPORT",    []() -> String { return device->animationSupported() ? "block" : "none"; } },
    {"ANIM_SOLID",          []() -> String { return (config.animation == SOLID) ? "selected" : ""; } },
    {"ANIM_RUNNING",        []() -> String { return device->runningSupported() ? (config.animation == RUN) ? "selected" : "" : "hidden"; } },
    {"ANIM_PULSING",        []() -> String { return device->pulsingSupported() ? (config.animation == PULSE) ? "selected" : "" : "hidden"; } },
    {"ANIM_BLINKING",       []() -> String { return device->blinkingSupported() ? (config.animation == BLINK) ? "selected" : "" : "hidden"; } },
};

// this function supplies template variables to the template engine
String processor(const String& var){
    auto item = processor_values.find(var.c_str());
    return item != processor_values.end() ? item->second() : String();
}

// if there is not specialization for the given result type, we assume the html parameter is a integer number and can
// be casted to the target type
template <typename T> T processParamConvert(AsyncWebParameter *p) { return (T)p->value().toInt();}

// for bool result type we check if the returned parameter is "on" -> true, otherwise false
template <> bool processParamConvert(AsyncWebParameter *p) { return p->value().equals("on");}

// for strings we return a string object
template <> std::string processParamConvert(AsyncWebParameter *p) { return p->value().c_str();}

template <typename T> bool processParam(AsyncWebParameter *p, const char* p_name, T& p_val)
{
    bool retval = false;
    if (p->name() == p_name)
    {
        T new_p_val = processParamConvert<T>(p);
        if (p_val != new_p_val)
        {
            Serial.printf("%s changed\n",p_name);
            p_val = new_p_val;
            retval = true;
        }
    }
    return retval;
}

void handleFSf ( AsyncWebServerRequest* request, const String& route ) {
    AsyncWebServerResponse *response ;
    bool saveNeeded = false;
    bool rebootNeeded = false;

    if ( route.indexOf ( "index.html" ) >= 0 ) // Index page is in PROGMEM
    {
        if (request->method() == HTTP_POST) {
            int params = request->params();
            bool mi_found = false;
            bool mo_found = false;
            for(int i=0;i<params;i++){
                AsyncWebParameter* p = request->getParam(i);
                Serial.printf("Parameter %s, value %s\r\n", p->name().c_str(), p->value().c_str());

                rebootNeeded |= processParam(p, "siteid", config.siteid);
                rebootNeeded |= processParam(p, "mqtt_host", config.mqtt_host);
                rebootNeeded |= processParam(p, "mqtt_pass", config.mqtt_pass);
                rebootNeeded |= processParam(p, "mqtt_user", config.mqtt_user);
                rebootNeeded |= processParam(p, "mqtt_port", config.mqtt_port);
                saveNeeded |= processParam(p, "mute_input", config.mute_input);
                saveNeeded |= processParam(p, "mute_output", config.mute_output);
                saveNeeded |= processParam(p, "amp_output", config.amp_output);
                saveNeeded |= processParam(p, "brightness", config.brightness);
                saveNeeded |= processParam(p, "hw_brightness", config.hotword_brightness);
                saveNeeded |= processParam(p, "hotword_detection", config.hotword_detection);
                saveNeeded |= processParam(p, "animation", config.animation);
                saveNeeded |= processParam(p, "gain", config.gain);
                saveNeeded |= processParam(p, "volume", config.volume);

                mi_found |= (p->name() == "mute_input");
                mo_found |= (p->name() == "mute_output");

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
            if (saveNeeded || rebootNeeded) {
                Serial.println("Settings changed, saving configuration");
                saveConfiguration(configfile, config);
                configChanged = true;
            } else {
                Serial.println("No settings changed");
            }
        }
        if (rebootNeeded) {
            response = request->beginResponse_P ( 200, "text/html", "<html><head><title>Rebooting...</title><script>setTimeout(function(){window.location.href = '/';},4000);</script></head><body><h1>Configuration saved, rebooting!</h1></body></html>");
        } else {
            response = request->beginResponse_P ( 200, "text/html", index_html, processor );
        }

    } else {
        response = request->beginResponse_P ( 200, "text/html", "<html><body>unkown route</body></html>") ;
    }

    request->send ( response ) ;
    
    if (rebootNeeded) {
        Serial.println("Rebooting!");
        ESP.restart();    
    }

}

void handleRequest ( AsyncWebServerRequest* request )
{
    handleFSf ( request, String( "/index.html") ) ;
}

void initHeader(int readSize, int width, int rate) {
    strncpy(header.riff_tag, "RIFF", 4);
    strncpy(header.wave_tag, "WAVE", 4);
    strncpy(header.fmt_tag, "fmt ", 4);
    strncpy(header.data_tag, "data", 4);

    header.riff_length = (uint32_t)sizeof(header) + (readSize * width);
    header.fmt_length = 16;
    header.audio_format = 1;
    header.num_channels = 1;
    header.sample_rate = rate;
    header.byte_rate = rate * width;
    header.block_align = width;
    header.bits_per_sample = width * 8;
    header.data_length = readSize * width;
}

void publishDebug(const char* message) {
    Serial.println(message);
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
    config.mqtt_host = MQTT_HOST;
  } else {
    serializeJsonPretty(doc, Serial);
    Serial.println();  
    config.siteid = doc.getMember("siteid").as<std::string>();
    config.mqtt_host = doc.getMember("mqtt_host").as<std::string>();
    config.mqtt_port = doc.getMember("mqtt_port").as<int>();
    config.mqtt_user = doc.getMember("mqtt_user").as<std::string>();
    config.mqtt_pass = doc.getMember("mqtt_pass").as<std::string>();
    config.mute_input = doc.getMember("mute_input").as<int>();
    config.mute_output = doc.getMember("mute_output").as<int>();
    config.amp_output = doc.getMember("amp_output").as<int>();
    device->ampOutput(config.amp_output);
    config.brightness = doc.getMember("brightness").as<int>();
    device->updateBrightness(config.brightness);
    config.hotword_brightness = doc.getMember("hotword_brightness").as<int>();
    //config.hotword_detection = doc.getMember("hotword_detection").as<int>();
    config.hotword_detection = 1;
    config.volume = doc.getMember("volume").as<int>();
    device->setVolume(config.volume);
    config.gain = doc.getMember("gain").as<int>();
    config.animation = doc.getMember("animation").as<int>();
    device->setGain(config.gain);
    audioFrameTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/audioFrame");
    playBytesTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/playBytes/#");
    playBytesStreamingTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/playBytesStreaming/#");
    playFinishedTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/playFinished");
    streamFinishedTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/streamFinished");
    audioTopic = config.siteid + std::string("/audio");
    ledTopic = config.siteid + std::string("/led");
    debugTopic = config.siteid + std::string("/debug");
    restartTopic = config.siteid + std::string("/restart");
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
    StaticJsonDocument<512> doc;
    doc["siteid"] = config.siteid;
    doc["mqtt_host"] = config.mqtt_host;
    doc["mqtt_port"] = config.mqtt_port;
    doc["mqtt_user"] = config.mqtt_user;
    doc["mqtt_pass"] = config.mqtt_pass;
    doc["mute_input"] = config.mute_input;
    doc["mute_output"] = config.mute_output;
    doc["amp_output"] = config.amp_output;
    doc["brightness"] = config.brightness;
    doc["hotword_brightness"] = config.hotword_brightness;
    doc["hotword_detection"] = config.hotword_detection;
    doc["volume"] = config.volume;
    doc["gain"] = config.gain;
    doc["animation"] = config.animation;
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
    }
    file.close();
}
