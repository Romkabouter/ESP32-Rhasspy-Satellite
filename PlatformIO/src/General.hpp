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
bool configChanged = false;

std::string audioFrameTopic("hermes/audioServer/" + config.siteid + "/audioFrame");
std::string playBytesTopic = "hermes/audioServer/" + config.siteid + "/playBytes/#";
std::string playFinishedTopic = "hermes/audioServer/" + config.siteid + "/playFinished";
std::string hotwordTopic = "hermes/hotword/#";
std::string audioTopic = config.siteid + std::string("/audio");
std::string ledTopic = config.siteid + std::string("/led");
std::string debugTopic = config.siteid + std::string("/debug");
std::string restartTopic = config.siteid + std::string("/restart");
std::string sayTopic = "hermes/tts/say";
std::string sayFinishedTopic = "hermes/tts/sayFinished";
std::string errorTopic = "hermes/nlu/intentNotRecognized";
std::string setVolumeTopic = "rhasspy/audioServer/setVolume";
AsyncMqttClient asyncClient; 
WiFiClient net;
PubSubClient audioServer(net); 
Esp32RingBuffer<uint8_t, uint16_t, (1U << 15)> audioData;
long message_size = 0;
int queueDelay = 10;
int sampleRate = 16000;
int numChannels = 2;
int bitDepth = 16;
StateColors current_colors = COLORS_IDLE;
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

void updateMqttTopicsStrings()
{
    audioFrameTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/audioFrame");
    playBytesTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/playBytes/#");
    playFinishedTopic = std::string("hermes/audioServer/") + config.siteid + std::string("/playFinished");
    audioTopic = config.siteid + std::string("/audio");
    ledTopic = config.siteid + std::string("/led");
    debugTopic = config.siteid + std::string("/debug");
    restartTopic = config.siteid + std::string("/restart");

}


//template<typename T> String toStringFunc(T val) { return String(val);}
template<typename T> String toStringFunc(T& val) { return String(val);}
template<> String toStringFunc(std::string& val) { return val.c_str();}
String toStringFunc(const char* val) { return String(val);}

// if there is not specialization for the given result type, we assume the html parameter is a integer number and can
// be casted to the target type
template <typename T> T processParamConvert(AsyncWebParameter *p) { return (T)p->value().toInt();}

// for bool result type we check if the returned parameter is "on" -> true, otherwise false
template <> bool processParamConvert(AsyncWebParameter *p) { return p->value().equals("on");}

// for strings we return a string object
template <> std::string processParamConvert(AsyncWebParameter *p) { return p->value().c_str();}

template <typename T> bool processParam(AsyncWebParameter *p, T& p_val)
{
    bool retval = false;
    T new_p_val = processParamConvert<T>(p);
    if (p_val != new_p_val)
    {
        p_val = new_p_val;
        retval = true;
    }
    return retval;
}

// these two gloval variables are used to simplify
// handling of "communication" out of the config parameter processing
// there more elegant solutions possible
static bool reconnectNeeded = false;
static bool doReconnect = false;

struct Conv 
{
    /**
     * @brief the function used to turn the parameter into its string representation
     * this is then used to replace all occurrences of %LABEL_IN_CAPS% in the index.html.h 
     * 
     */ 
    String (*toWebString)();

    /**
     * @brief the pointer to the function to  save the website parameter into the config data.
     * if not relevant, just return false as lambda function will do or leave default.
     *  no op template: [](AsyncWebParameter *p) { return false; } 
     *  otherwise [](AsyncWebParameter *p) { return processParam(p, config.NAME_OF_FIELD); }
     * 
     * @param p the parameter to process from the web server request 
     * @returns true if value was changed, false otherwise
     */
    bool   (*toValue)(AsyncWebParameter* p) ;

    /**
     * @brief the pointer to function to be called if the parameter was changed during configuration, used to apply the value
     * specify empty lambda function if not directly applicable: []() {}
     */
    void   (*apply)();
};

const std::map<const std::string, Conv> config_values = {
    { "siteid", { 
            []() { return toStringFunc(config.siteid); },
            [](AsyncWebParameter *p) { return processParam(p, config.siteid); },
            []() { reconnectNeeded = true; updateMqttTopicsStrings(); } 
        }
    },
    { "mqtt_host", { 
            []() { return toStringFunc(config.mqtt_host); },
            [](AsyncWebParameter *p) { return processParam(p, config.mqtt_host); },
            []() { reconnectNeeded = true; } 
        }
    },
    { "mqtt_port", { 
            []() { return toStringFunc(config.mqtt_port); },
            [](AsyncWebParameter *p) { return processParam(p, config.mqtt_port); },
            []() { reconnectNeeded = true; } 
        }
    },
    { "mqtt_user", { 
            []() { return toStringFunc(config.mqtt_user); },
            [](AsyncWebParameter *p) { return processParam(p, config.mqtt_user); },
            []() { reconnectNeeded = true; } 
        }
    },
    { "mqtt_pass", { 
            []() { return toStringFunc(config.mqtt_pass); },
            [](AsyncWebParameter *p) { return processParam(p, config.mqtt_pass); },
            []() { reconnectNeeded = true; } 
        }
    },
    { "volume", { 
            []() { return toStringFunc(config.volume); },
            [](AsyncWebParameter *p) { return processParam(p, config.volume); },
            []() { device->setVolume(config.volume); } 
        }
    },
    { "gain", { 
            []() { return toStringFunc(config.gain); },
            [](AsyncWebParameter *p) { return processParam(p, config.gain); },
            []() { device->setGain(config.gain); } 
        }
    },
    { "brightness", { 
            []() { return toStringFunc(config.brightness); },
            [](AsyncWebParameter *p) { return processParam(p, config.brightness); },
            []() { device->updateBrightness(config.brightness); } 
        }
    },
    { "hw_brightness", { 
            []() { return toStringFunc(config.hotword_brightness); },
            [](AsyncWebParameter *p) { return processParam(p, config.hotword_brightness); },
            []() {} 
        }
    },
    { "mute_input", { 
            []() { return toStringFunc((config.mute_input) ? "checked" : ""); },
            [](AsyncWebParameter *p) { return processParam(p, config.mute_input); },
            []() {} 
        }
    },
    { "mute_output", { 
            []() { return toStringFunc((config.mute_output) ? "checked" : ""); },
            [](AsyncWebParameter *p) { return processParam(p, config.mute_output); },
            []() {} 
        }
    },
    { "amp_output", { 
            []() { return toStringFunc(config.amp_output); },
            [](AsyncWebParameter *p) { return processParam(p, config.amp_output); },
            []() { device->ampOutput(config.amp_output); } 
        }
    },
    { "hotword_detection", { 
            []() { return toStringFunc(config.hotword_detection); },
            [](AsyncWebParameter *p) { return processParam(p,config.hotword_detection); },
            []() {} 
        }
    },
    { "hw_local", { 
            []() { return toStringFunc((config.hotword_detection == HW_LOCAL) ? "selected" : ""); },
            [](AsyncWebParameter *p) { return false; },
            []() {} 
        }
    },
    { "hw_remote", { 
            []() { return toStringFunc((config.hotword_detection == HW_REMOTE) ? "selected" : ""); },
            [](AsyncWebParameter *p) { return false; },
            []() {} 
        }
    },
    { "amp_out_speakers", { 
            []() { return toStringFunc(device->numAmpOutConfigurations() < 1? "hidden" : (config.amp_output == AMP_OUT_SPEAKERS) ? "selected" : ""); },
            [](AsyncWebParameter *p) { return false; },
            []() {} 
        }
    },
    { "amp_out_headphone", { 
            []() { return toStringFunc(device->numAmpOutConfigurations() < 2? "hidden" : (config.amp_output == AMP_OUT_HEADPHONE) ? "selected" : ""); },
            [](AsyncWebParameter *p) { return false; },
            []() {} 
        }
    },
    { "amp_out_both", { 
            []() { return toStringFunc(device->numAmpOutConfigurations() < 3? "hidden" : (config.amp_output == AMP_OUT_BOTH) ? "selected" : ""); },
            [](AsyncWebParameter *p) { return false; },
            []() {} 
        }
    },
    {"animationsupported",  {   
            []() -> String { return device->animationSupported() ? "block" : "none"; },
            [](AsyncWebParameter *p) { return false; },
            []() {} 
        }
    },
    { "animation", { 
            []() { return toStringFunc(config.animation); },
            [](AsyncWebParameter *p) { return processParam(p, config.animation); },
            []() {} 
        }
    },
    {"anim_solid", {
            []() -> String { return (config.animation == SOLID) ? "selected" : ""; },
            [](AsyncWebParameter *p) { return false; },
            []() {}  
        }
    },
    {"anim_running", {
            []() -> String { return device->runningSupported() ? (config.animation == RUN) ? "selected" : "" : "hidden"; },
            [](AsyncWebParameter *p) { return false; },
            []() {} 
        } 
    },
    {"anim_pulsing", {
            []() -> String { return device->pulsingSupported() ? (config.animation == PULSE) ? "selected" : "" : "hidden"; },
            [](AsyncWebParameter *p) { return false; },
            []() {} 
        } 
    },
    {"anim_blinking", {      
            []() -> String { return device->blinkingSupported() ? (config.animation == BLINK) ? "selected" : "" : "hidden"; },
            [](AsyncWebParameter *p) { return false; },
            []() {} 
        }
    },
};

// this function supplies template variables to the template engine
String processor2(const String& _var){
    std::string var(_var.c_str());
    std::for_each(var.begin(), var.end(), [](char & c) {
        c = ::tolower(c);
    });

    auto item = config_values.find(var.c_str());
    return item != config_values.end() ? item->second.toWebString() : String();
}

void handleFSf ( AsyncWebServerRequest* request, const String& route ) {
    AsyncWebServerResponse *response ;
    bool saveNeeded = false;

    if ( route.indexOf ( "index.html" ) >= 0 ) // Index page is in PROGMEM
    {
        if (request->method() == HTTP_POST) {
            for(auto config_value: config_values)
            {
                AsyncWebParameter* p = request->getParam(config_value.first.c_str(), true);
                if (p)
                {
                  bool sn = config_value.second.toValue(p);
                  Serial.printf("Parameter %s, value %s save? = %d\r\n", p->name().c_str(), p->value().c_str(), sn);
                  if (sn) { config_value.second.apply(); }
                  saveNeeded |= sn;
                }
            }

            if (saveNeeded || reconnectNeeded) {
                Serial.println("Settings changed, saving configuration");
                saveConfiguration(configfile, config);
                configChanged = true;
            } else {
                Serial.println("No settings changed");
            }
        }
        if (reconnectNeeded) {
            response = request->beginResponse_P ( 200, "text/html", "<html><head><title>Reconnecting...</title><script>setTimeout(function(){window.location.href = '/';},4000);</script></head><body><h1>Configuration saved, reconnecting using changed settings!</h1></body></html>");
        } else {
            response = request->beginResponse_P ( 200, "text/html", index_html, processor2 );
        }

    } else {
        response = request->beginResponse_P ( 200, "text/html", "<html><body>unkown route</body></html>") ;
    }
    
    request->send ( response ) ;
    
    if (reconnectNeeded) {
        Serial.println("Reconnecting!");
        delay(1000);
        reconnectNeeded = false;
        doReconnect = true;
        // ESP.restart();    
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
    config.brightness = doc.getMember("brightness").as<int>();
    config.hotword_brightness = doc.getMember("hotword_brightness").as<int>();
    config.hotword_detection = doc.getMember("hotword_detection").as<int>();
    config.volume = doc.getMember("volume").as<int>();
    config.gain = doc.getMember("gain").as<int>();
    config.animation = doc.getMember("animation").as<int>();

    // apply configuration values
    device->ampOutput(config.amp_output);
    device->updateBrightness(config.brightness);
    device->setVolume(config.volume);
    device->setGain(config.gain);
    
    // reconfigure if siteid changes
    updateMqttTopicsStrings();

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
