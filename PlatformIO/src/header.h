#include <tinyfsm.hpp>
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <PubSubClient.h>
#include "RingBuf.h"

#define READ_SIZE 256
#define WRITE_SIZE 256
#define WIDTH 2
#define RATE 16000

const int PLAY = BIT0;
const int STREAM = BIT1;

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
int retryCount = 0;
int I2SMode = -1;
bool mqttConnected = false;
std::string audioFrameTopic = std::string("hermes/audioServer/") + SITEID + std::string("/audioFrame");
std::string playBytesTopic = std::string("hermes/audioServer/") + SITEID + std::string("/playBytes/#");
std::string hotwordTopic = "hermes/hotword/#";
std::string debugTopic = SITEID + std::string("/debug");
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

struct WifiDisconnected;
struct MQTTDisconnected;
struct HotwordDetected;
struct StreamAudio;
struct PlayAudio;

struct WifiDisconnectEvent : tinyfsm::Event { };
struct WifiConnectEvent : tinyfsm::Event { };
struct MQTTDisconnectedEvent : tinyfsm::Event { };
struct MQTTConnectedEvent : tinyfsm::Event { };
struct StreamAudioEvent : tinyfsm::Event { };
struct PlayAudioEvent : tinyfsm::Event { };
struct HotwordDetectedEvent : tinyfsm::Event { };

void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void publishDebug(const char* message);
void InitI2SSpeakerOrMic(int mode);
void WiFiEvent(WiFiEvent_t event);
void initHeader();
void MQTTtask(void *p);
void I2Stask(void *p);

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
