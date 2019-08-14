/* ************************************************************************* *
   Matrix Voice Audio Streamer

   This program is written to be a streaming audio server running on the Matrix
 Voice. This is typically used for Snips.AI, it will then be able to replace the
 Snips Audio Server, by publishing small wave messages to the hermes protocol
   See https://snips.ai/ for more information

   Author:  Paul Romkes
   Date:    September 2018
   Version: 3.3

   Changelog:
   ==========
   v1:
    - first code release. It needs a lot of improvement, no hardcoding stuff
   v2:
    - Change to Arduino IDE
   v2.1:
    - Changed to pubsubclient and fixed other stability issues
   v3:
    - Add OTA
   v3.1:
    - Only listen to SITEID to toggle hotword
    - Got rid of String, leads to Heap Fragmentation
    - Add dynamic brihtness, post {"brightness": 50 } to SITEID/everloop
    - Fix stability, using semaphores
   v3.2:
    - Add dynamic colors, see readme for documentation
    - Restart the device by publishing hashed password to SITEID/restart
    - Adjustable framerate, more info at
 https://snips.gitbook.io/documentation/advanced-configuration/platform-configuration
    - Rotating animation possible, not finished or used yet
   v3.3:
    - Added support for Rhasspy https://github.com/synesthesiam/rhasspy
    - Started implementing playBytes, not finished
   v3.4:
    - Implemented playBytes, basics done but sometimes audio garbage out
   v4.0:
    - playBytes working, only plays 44100 samplerate (mono/stereo) correctly.
 Work in progress
    - Upgrade to ArduinoJSON 6
    - Add mute/unmute via MQTT
    - Fixed OTA issues, remove webserver
   v4.1:
    - Configurable mic gain
    - Fix on only listening to Dutch Rhasspy
 * ************************************************************************ */

#include <Arduino.h>

#include <chrono>
#include <string>
#include <thread>

#include <ArduinoOTA.h>
#include <WiFi.h>

#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <PubSubClient.h>
#include "RingBuf.h"

#include "everloop.h"
#include "everloop_image.h"
#include "microphone_array.h"
#include "microphone_core.h"
#include "voice_memory_map.h"
#include "wishbone_bus.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
}

// User configuration in platformio.ini

/* ************************************************************************* *
      DEFINES AND GLOBALS
 * ************************************************************************ */
#define RATE 16000
#define WIDTH 2
#define CHANNELS 1
#define DATA_CHUNK_ID 0x61746164
#define FMT_CHUNK_ID 0x20746d66
// Convert 4 byte little-endian to a long,
#define longword(bfr, ofs) \
    (bfr[ofs + 3] << 24 | bfr[ofs + 2] << 16 | bfr[ofs + 1] << 8 | bfr[ofs + 0])
#define shortword(bfr, ofs) (bfr[ofs + 1] << 8 | bfr[ofs + 0])

// Matrix Voice
namespace hal = matrix_hal;
static hal::WishboneBus wb;
static hal::Everloop everloop;
static hal::MicrophoneArray mics;
static hal::EverloopImage image1d;
WiFiClient net;
AsyncMqttClient asyncClient;    // ASYNCH client to be able to handle huge
                                // messages like WAV files
PubSubClient audioServer(net);  // We also need a sync client, asynch leads to
                                // errors on the audio thread
// Timers
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
TaskHandle_t audioStreamHandle;
TaskHandle_t audioPlayHandle;
TaskHandle_t everloopTaskHandle;
SemaphoreHandle_t wbSemaphore;
// Globals
const int kMaxWriteLength = 1024;
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
static struct wavfile_header header;
const int EVERLOOP = BIT0;
const int ANIMATE = BIT1;
const int PLAY = BIT2;
const int STREAM = BIT3;
int hotword_colors[4] = {0, 255, 0, 0};
int idle_colors[4] = {0, 0, 255, 0};
int wifi_disc_colors[4] = {255, 0, 0, 0};
int audio_disc_colors[4] = {255, 0, 0, 255};
int update_colors[4] = {0, 0, 0, 255};
int brightness = 15;
long lastReconnectAudio = 0;
long lastCounterTick = 0;
int streamMessageCount = 0;
long message_size, elapsed, start = 0;
RingBuf<uint8_t, 1024 * 4> audioData;
bool sendAudio = true;
bool audioOK = true;
bool wifi_connected = false;
bool hotword_detected = false;
bool isUpdateInProgess = false;
std::string finishedMsg = "";
int message_count;
int CHUNK = 256;  // set to multiplications of 256, voice return a set of 256
int chunkValues[] = {32, 64, 128, 256, 512, 1024};
static EventGroupHandle_t everloopGroup;
static EventGroupHandle_t audioGroup;
// This is used to be able to change brightness, while keeping the colors appear
// the same Called gamma correction, check this
// https://learn.adafruit.com/led-tricks-gamma-correction/the-issue
const uint8_t PROGMEM gamma8[] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,
    2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,
    4,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,
    8,   9,   9,   9,   10,  10,  10,  11,  11,  11,  12,  12,  13,  13,  13,
    14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  20,  21,
    21,  22,  22,  23,  24,  24,  25,  25,  26,  27,  27,  28,  29,  29,  30,
    31,  32,  32,  33,  34,  35,  35,  36,  37,  38,  39,  39,  40,  41,  42,
    43,  44,  45,  46,  47,  48,  49,  50,  50,  51,  52,  54,  55,  56,  57,
    58,  59,  60,  61,  62,  63,  64,  66,  67,  68,  69,  70,  72,  73,  74,
    75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89,  90,  92,  93,  95,
    96,  98,  99,  101, 102, 104, 105, 107, 109, 110, 112, 114, 115, 117, 119,
    120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142, 144, 146,
    148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175, 177,
    180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
    215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252,
    255};

/* ************************************************************************* *
      MQTT TOPICS
 * ************************************************************************ */
// Dynamic topics for MQTT
std::string audioFrameTopic =
    std::string("hermes/audioServer/") + SITEID + std::string("/audioFrame");
std::string playFinishedTopic =
    std::string("hermes/audioServer/") + SITEID + std::string("/playFinished");
std::string playBytesTopic =
    std::string("hermes/audioServer/") + SITEID + std::string("/playBytes/#");
std::string rhasspyWakeTopic = std::string("rhasspy/+/transition/+");
std::string toggleOffTopic = "hermes/hotword/toggleOff";
std::string toggleOnTopic = "hermes/hotword/toggleOn";
std::string everloopTopic = SITEID + std::string("/everloop");
std::string debugTopic = SITEID + std::string("/debug");
std::string audioTopic = SITEID + std::string("/audio");
std::string restartTopic = SITEID + std::string("/restart");

/* ************************************************************************* *
      HELPER CLASS FOR WAVE HEADER, taken from https://www.xtronical.com/
      Changed to fit my needs
 * ************************************************************************ */
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
    unsigned long ofs, siz;
    ofs = 12;
    siz = longword(WavData, 4);
    SampleRate = DataStart = 0;
    while (ofs < siz) {
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

/* ************************************************************************* *
      NETWORK FUNCTIONS AND MQTT
 * ************************************************************************ */
void connectToWifi() {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void connectToMqtt() {
    Serial.println("Connecting to asynch MQTT...");
    asyncClient.connect();
}

bool connectAudio() {
    Serial.println("Connecting to synch MQTT...");
    if (audioServer.connect("MatrixVoiceAudio", MQTT_USER, MQTT_PASS)) {
        Serial.println("Connected to synch MQTT!");
        if (asyncClient.connected()) {
            asyncClient.publish(debugTopic.c_str(), 0, false,
                                "Connected to synch MQTT!");
        }
    }
    return audioServer.connected();
}

// ---------------------------------------------------------------------------
// WIFI event
// Kicks off various stuff in case of connect/disconnect
// ---------------------------------------------------------------------------
void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_STA_GOT_IP:
            wifi_connected = true;
            xEventGroupSetBits(
                everloopGroup,
                EVERLOOP);  // Set the bit so the everloop gets updated
            connectToMqtt();
            connectAudio();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            wifi_connected = false;
            xEventGroupSetBits(everloopGroup, EVERLOOP);
            xTimerStop(
                mqttReconnectTimer,
                0);  // Do not reconnect to MQTT while reconnecting to network
            xTimerStart(wifiReconnectTimer, 0);  // Start the reconnect timer
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
    asyncClient.subscribe(toggleOffTopic.c_str(), 0);
    asyncClient.subscribe(toggleOnTopic.c_str(), 0);
    asyncClient.subscribe(rhasspyWakeTopic.c_str(), 0);
    asyncClient.subscribe(everloopTopic.c_str(), 0);
    asyncClient.subscribe(restartTopic.c_str(), 0);
    asyncClient.subscribe(audioTopic.c_str(), 0);
    asyncClient.subscribe(debugTopic.c_str(), 0);
    asyncClient.publish(debugTopic.c_str(), 0, false,
                        "Connected to asynch MQTT!");
    // xEventGroupClearBits(everloopGroup, ANIMATE);
}

// ---------------------------------------------------------------------------
// MQTT Disonnect event
// ---------------------------------------------------------------------------
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.println("Disconnected from MQTT.");
    if (!isUpdateInProgess) {
        // xEventGroupSetBits(everloopGroup, ANIMATE);
        if (WiFi.isConnected()) {
            xTimerStart(mqttReconnectTimer, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// MQTT Callback
// Handles messages for various topics
// ---------------------------------------------------------------------------
void onMqttMessage(char *topic, char *payload,
                   AsyncMqttClientMessageProperties properties, size_t len,
                   size_t index, size_t total) {
    std::string topicstr(topic);
    message_size = total;
    if (len + index == total) {
        // when len + index is total, we have reached the end of the message.
        // We can then do work on it
        if (topicstr.find("toggleOff") != std::string::npos) {
            std::string payloadstr(payload);
            // Check if this is for us
            if (payloadstr.find(SITEID) != std::string::npos) {
                hotword_detected = true;
                xEventGroupSetBits(
                    everloopGroup,
                    EVERLOOP);  // Set the bit so the everloop gets updated
            }
        } else if (topicstr.find("toggleOn") != std::string::npos) {
            // Check if this is for us
            std::string payloadstr(payload);
            if (payloadstr.find(SITEID) != std::string::npos) {
                hotword_detected = false;
                xEventGroupSetBits(
                    everloopGroup,
                    EVERLOOP);  // Set the bit so the everloop gets updated
            }
        } else if (topicstr.find("WakeListener") != std::string::npos) {
            std::string payloadstr(payload);
            if (payloadstr.find("started") != std::string::npos ||
                payloadstr.find("loaded") != std::string::npos) {
                hotword_detected = true;
                xEventGroupSetBits(
                    everloopGroup,
                    EVERLOOP);  // Set the bit so the everloop gets updated
            }
            if (payloadstr.find("listening") != std::string::npos) {
                hotword_detected = false;
                xEventGroupSetBits(
                    everloopGroup,
                    EVERLOOP);  // Set the bit so the everloop gets updated
            }
        } else if (topicstr.find("playBytes") != std::string::npos) {
            elapsed = millis() - start;
            char str[100];
            sprintf(str, "Received in %d ms", (int)elapsed);
            asyncClient.publish(debugTopic.c_str(), 0, false, str);
            // Get the ID from the topic
            finishedMsg =
                "{\"id\":\"" + topicstr.substr(19 + strlen(SITEID) + 11, 37) +
                "\",\"siteId\":\"" + SITEID + "\",\"sessionId\":null}";
            for (int i = 0; i < len; i++) {
                while (!audioData.push((uint8_t)payload[i])) {
                    delay(1);
                }
                if (audioData.isFull() &&
                    xEventGroupGetBits(audioGroup) != PLAY) {
                    xEventGroupClearBits(audioGroup, STREAM);
                    xEventGroupSetBits(audioGroup, PLAY);
                }
            }
        } else if (topicstr.find(everloopTopic.c_str()) != std::string::npos) {
            std::string payloadstr(payload);
            StaticJsonDocument<300> doc;
            DeserializationError err = deserializeJson(doc, payloadstr.c_str());
            if (!err) {
                JsonObject root = doc.as<JsonObject>();
                if (root.containsKey("brightness")) {
                    // all values below 10 is read as 0 in gamma8, we map 0 to
                    // 10
                    brightness = (int)(root["brightness"]) * 90 / 100 + 10;
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
                if (root.containsKey("wifi_disconnect")) {
                    wifi_disc_colors[0] = root["wifi_disconnect"][0];
                    wifi_disc_colors[1] = root["wifi_disconnect"][1];
                    wifi_disc_colors[2] = root["wifi_disconnect"][2];
                    wifi_disc_colors[3] = root["wifi_disconnect"][3];
                }
                if (root.containsKey("update")) {
                    update_colors[0] = root["update"][0];
                    update_colors[1] = root["update"][1];
                    update_colors[2] = root["update"][2];
                    update_colors[3] = root["update"][3];
                }
                xEventGroupSetBits(everloopGroup, EVERLOOP);
            } else {
                asyncClient.publish(debugTopic.c_str(), 0, false, err.c_str());
            }
        } else if (topicstr.find(audioTopic.c_str()) != std::string::npos) {
            std::string payloadstr(payload);
            StaticJsonDocument<300> doc;
            DeserializationError err = deserializeJson(doc, payloadstr.c_str());
            if (!err) {
                JsonObject root = doc.as<JsonObject>();
                if (root.containsKey("framerate")) {
                    bool found = false;
                    for (int i = 0; i < 6; i++) {
                        if (chunkValues[i] == root["framerate"]) {
                            CHUNK = root["framerate"];
                            message_count =
                                (int)round(mics.NumberOfSamples() / CHUNK);
                            header.riff_length =
                                (uint32_t)sizeof(header) + (CHUNK * WIDTH);
                            header.data_length = CHUNK * WIDTH;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        asyncClient.publish(
                            debugTopic.c_str(), 0, false,
                            "Framerate should be 32,64,128,256,512 or 1024");
                    }
                }
                if (root.containsKey("mute")) {
                    // feels more intuitive to send mute
                    sendAudio = (root["mute"] == "on") ? false : true;
                }
                if (root.containsKey("gain")) {
                    // feels more intuitive to send mute
                    mics.SetGain((int)root["gain"]);
                }
            } else {
                asyncClient.publish(debugTopic.c_str(), 0, false, err.c_str());
            }
        } else if (topicstr.find(restartTopic.c_str()) != std::string::npos) {
            std::string payloadstr(payload);
            StaticJsonDocument<300> doc;
            DeserializationError err = deserializeJson(doc, payloadstr.c_str());
            if (!err) {
                JsonObject root = doc.as<JsonObject>();
                if (root.containsKey("passwordhash")) {
                    if (root["passwordhash"] == OTA_PASS_HASH) {
                        ESP.restart();
                    }
                }
            } else {
                asyncClient.publish(debugTopic.c_str(), 0, false, err.c_str());
            }
        }
    } else {
        // len + index < total ==> partial message
        if (topicstr.find("playBytes") != std::string::npos) {
            if (index == 0) {
                start = millis();
                elapsed = millis();
                audioData.clear();
            }
            for (int i = 0; i < len; i++) {
                while (!audioData.push((uint8_t)payload[i])) {
                    delay(1);
                }
                if (audioData.isFull() &&
                    xEventGroupGetBits(audioGroup) != PLAY) {
                    xEventGroupClearBits(audioGroup, STREAM);
                    xEventGroupSetBits(audioGroup, PLAY);
                }
            }
        }
    }
}

/* ************************************************************************* *
      AUDIOSTREAM TASK, USES SYNCED MQTT CLIENT
 * ************************************************************************ */
void Audiostream(void *p) {
    while (1) {
        // Wait for the bit before updating. Do not clear in the wait exit;
        // (first false)
        xEventGroupWaitBits(audioGroup, STREAM, false, false, portMAX_DELAY);
        // See if we can obtain or "Take" the Serial Semaphore.
        // If the semaphore is not available, wait 5 ticks of the Scheduler to
        // see if it becomes free.
        if (sendAudio && audioServer.connected() &&
            (xSemaphoreTake(wbSemaphore, (TickType_t)5000) == pdTRUE)) {
            // We are connected, make sure there is no overlap with the STREAM
            // bit
            if (xEventGroupGetBits(audioGroup) != PLAY) {
                mics.Read();

                // Sound buffers
                uint16_t voicebuffer[CHUNK];
                uint8_t voicemapped[CHUNK * WIDTH];
                uint8_t payload[sizeof(header) + (CHUNK * WIDTH)];

                // Message count is the Mattix NumberOfSamples divided by the
                // framerate of Snips. This defaults to 512 / 256 = 2. If you
                // lower the framerate, the AudioServer has to send more
                // wavefile because the NumOfSamples is a fixed number
                for (int i = 0; i < message_count; i++) {
                    for (uint32_t s = CHUNK * i; s < CHUNK * (i + 1); s++) {
                        voicebuffer[s - (CHUNK * i)] = mics.Beam(s);
                    }
                    // voicebuffer will hold 256 samples of 2 bytes, but we need
                    // it as 1 byte We do a memcpy, because I need to add the
                    // wave header as well
                    memcpy(voicemapped, voicebuffer, CHUNK * WIDTH);

                    // Add the wave header
                    memcpy(payload, &header, sizeof(header));
                    memcpy(&payload[sizeof(header)], voicemapped,
                           sizeof(voicemapped));
                    audioServer.publish(audioFrameTopic.c_str(),
                                        (uint8_t *)payload, sizeof(payload));
                    streamMessageCount++;
                }
            }
            xSemaphoreGive(
                wbSemaphore);  // Now free or "Give" the Serial Port for others.
        }
        vTaskDelay(1);
    }
    vTaskDelete(NULL);
}

/* ************************************************************************ *
      LED ANIMATION TASK
 * ************************************************************************ */
void everloopAnimation(void *p) {
    int position = 0;
    int red;
    int green;
    int blue;
    int white;
    while (1) {
        xEventGroupWaitBits(everloopGroup, ANIMATE, true, true,
                            portMAX_DELAY);  // Wait for the bit before updating
        if (xSemaphoreTake(wbSemaphore, (TickType_t)5000) == pdTRUE) {
            for (int i = 0; i < image1d.leds.size(); i++) {
                red = ((i + 1) * brightness / image1d.leds.size()) *
                      idle_colors[0] / 100;
                green = ((i + 1) * brightness / image1d.leds.size()) *
                        idle_colors[1] / 100;
                blue = ((i + 1) * brightness / image1d.leds.size()) *
                       idle_colors[2] / 100;
                white = ((i + 1) * brightness / image1d.leds.size()) *
                        idle_colors[3] / 100;
                image1d.leds[(i + position) % image1d.leds.size()].red =
                    pgm_read_byte(&gamma8[red]);
                image1d.leds[(i + position) % image1d.leds.size()].green =
                    pgm_read_byte(&gamma8[green]);
                image1d.leds[(i + position) % image1d.leds.size()].blue =
                    pgm_read_byte(&gamma8[blue]);
                image1d.leds[(i + position) % image1d.leds.size()].white =
                    pgm_read_byte(&gamma8[white]);
            }
            position++;
            position %= image1d.leds.size();
            everloop.Write(&image1d);
            delay(50);
            xSemaphoreGive(wbSemaphore);  // Free for all
        }
    }
    vTaskDelete(NULL);
}

/* ************************************************************************ *
      LED RING TASK
 * ************************************************************************ */
void everloopTask(void *p) {
    while (1) {
        xEventGroupWaitBits(everloopGroup, EVERLOOP, false, false,
                            portMAX_DELAY);
        Serial.println("Updating everloop");
        // Implementation of Semaphore, otherwise the ESP will crash due to read
        // of the mics Wait a really long time to make sure we get access (10000
        // ticks)
        if (xSemaphoreTake(wbSemaphore, (TickType_t)10000) == pdTRUE) {
            // Yeah got it, see what colors we need
            int r = 0;
            int g = 0;
            int b = 0;
            int w = 0;
            if (isUpdateInProgess) {
                r = update_colors[0];
                g = update_colors[1];
                b = update_colors[2];
                w = update_colors[3];
            } else if (hotword_detected) {
                r = hotword_colors[0];
                g = hotword_colors[1];
                b = hotword_colors[2];
                w = hotword_colors[3];
            } else if (!wifi_connected) {
                r = wifi_disc_colors[0];
                g = wifi_disc_colors[1];
                b = wifi_disc_colors[2];
                w = wifi_disc_colors[3];
            } else if (!audioOK) {
                r = audio_disc_colors[0];
                g = audio_disc_colors[1];
                b = audio_disc_colors[2];
                w = audio_disc_colors[3];
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
            for (hal::LedValue &led : image1d.leds) {
                led.red = r;
                led.green = g;
                led.blue = b;
                led.white = w;
            }
            everloop.Write(&image1d);
            xSemaphoreGive(wbSemaphore);  // Free for all
            xEventGroupClearBits(everloopGroup,
                                 EVERLOOP);  // Clear the everloop bit
            Serial.println("Updating done");
        }
        vTaskDelay(1);  // Delay a tick, for better stability
    }
    vTaskDelete(NULL);
}

/* ************************************************************************ *
      AUDIO OUTPUT TASK
 * ************************************************************************ */
void MakeStereo(uint16_t buf[], const int len) {
    for (int i = len / 2 - 1, j = len - 1; i > 0; --i) {
        buf[j--] = buf[i];
        buf[j--] = buf[i];
    }
}

void AudioPlayTask(void *p) {
    while (1) {
        // Wait for the bit before updating, do not clear when exit wait
        xEventGroupWaitBits(audioGroup, PLAY, false, false, portMAX_DELAY);
        // clear the stream bit (makes the stream stop
        xEventGroupClearBits(audioGroup, STREAM);
        if (xSemaphoreTake(wbSemaphore, (TickType_t)5000) == pdTRUE) {
            Serial.println("Play Audio");
            asyncClient.publish(debugTopic.c_str(), 0, false, "Play files");
            const int kMaxWriteLength = 1024;
            float sleep =
                1000000 / (16 / 8 * 44100 * 2 /
                           (kMaxWriteLength / 2));  // 2902,494331065759637
            sleep = 4000;                           // sounds better?
            // Use CircularBuffer
            int played = 0;  // Skip header
            uint8_t WaveData[44];
            for (int k = 0; k < 44; k++) {
                audioData.pop(WaveData[k]);
                played++;
            }
            // class to get a waveheader
            XT_Wav_Class Message((const uint8_t *)WaveData);
            char str[100];
            sprintf(str,
                    "Samplerate: %d, Channels: %d, Format: %04X, Bits per "
                    "Sample: %04X",
                    (int)Message.SampleRate, (int)Message.NumChannels,
                    (int)Message.Format, (int)Message.BitsPerSample);
            asyncClient.publish(debugTopic.c_str(), 0, false, str);
            while (played < message_size) {
                int bytes_to_read = kMaxWriteLength;
                if (message_size - played < kMaxWriteLength) {
                    bytes_to_read = message_size - played;
                }

                uint8_t data[bytes_to_read];
                while (audioData.size() < bytes_to_read &&
                       played < message_size) {
                    vTaskDelay(1);
                }
                for (int i = 0; i < bytes_to_read; i++) {
                    audioData.pop(data[i]);
                }
                played = played + bytes_to_read;

                // convert the orginal data to 16bit buffer, so we can work with
                // it if needed (resampling, make stereo)

                if (Message.NumChannels == 1) {
                    uint16_t dataS[bytes_to_read];
                    int samples = 0;

                    for (int i = 0; i < bytes_to_read - 2; i += 2) {
                        // dataS[samples] = ((data[i] & 0xff) | (data[i + 1] <<
                        // 8));
                        dataS[samples] =
                            shortword(data, i);  // make stereo out of mono
                        dataS[samples + 1] = dataS[samples];
                        if (Message.SampleRate == 16000) {
                            // increase sample rate from 16000 to 48000 by
                            // factor x3
                            dataS[samples + 2] = dataS[samples];
                            dataS[samples + 3] = dataS[samples];
                            dataS[samples + 4] = dataS[samples];
                            dataS[samples + 5] = dataS[samples];
                        }
                        samples += 6;
                        if (sizeof(uint16_t) * samples >= kMaxWriteLength) {
                            wb.SpiWrite(hal::kDACBaseAddress,
                                        (const uint8_t *)dataS,
                                        kMaxWriteLength);
                            std::this_thread::sleep_for(
                                std::chrono::microseconds((int)sleep));
                            samples = 0;
                        }
                    }
                    wb.SpiWrite(hal::kDACBaseAddress, (const uint8_t *)dataS,
                                sizeof(uint16_t) * samples);
                    std::this_thread::sleep_for(
                        std::chrono::microseconds((int)sleep));
                } else {
                    wb.SpiWrite(hal::kDACBaseAddress, (const uint8_t *)data,
                                sizeof(data));
                    std::this_thread::sleep_for(
                        std::chrono::microseconds((int)sleep));
                }
            }
            asyncClient.publish(playFinishedTopic.c_str(), 0, false,
                                finishedMsg.c_str());
            asyncClient.publish(debugTopic.c_str(), 0, false, "Done!");
            audioData.clear();
        }
        xEventGroupClearBits(audioGroup, PLAY);
        xSemaphoreGive(wbSemaphore);
        xEventGroupSetBits(audioGroup, STREAM);
    }
    vTaskDelete(NULL);
}

/* ************************************************************************ *
      SETUP
 * ************************************************************************ */
void setup() {
    // Implementation of Semaphore, otherwise the ESP will crash due to read of
    // the mics
    if (wbSemaphore == NULL)  // Not yet been created?
    {
        wbSemaphore = xSemaphoreCreateMutex();  // Create a mutex semaphore
        if ((wbSemaphore) != NULL) xSemaphoreGive(wbSemaphore);  // Free for all
    }

    // Reconnect timers
    mqttReconnectTimer =
        xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                     reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
    wifiReconnectTimer =
        xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                     reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

    WiFi.onEvent(WiFiEvent);
    asyncClient.setClientId("MatrixVoice");
    asyncClient.onConnect(onMqttConnect);
    asyncClient.onDisconnect(onMqttDisconnect);
    asyncClient.onMessage(onMqttMessage);
    asyncClient.setServer(MQTT_IP, MQTT_PORT);
    asyncClient.setCredentials(MQTT_USER, MQTT_PASS);
    audioServer.setServer(MQTT_IP, 1883);

    everloopGroup = xEventGroupCreate();
    audioGroup = xEventGroupCreate();

    strncpy(header.riff_tag, "RIFF", 4);
    strncpy(header.wave_tag, "WAVE", 4);
    strncpy(header.fmt_tag, "fmt ", 4);
    strncpy(header.data_tag, "data", 4);

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

    // setup mics
    mics.Setup(&wb);
    mics.SetSamplingRate(RATE);

    // Microphone Core Init
    hal::MicrophoneCore mic_core(mics);
    mic_core.Setup(&wb);

    // NumberOfSamples() = kMicarrayBufferSize / kMicrophoneChannels = 4069 / 8
    // = 512 Depending on the CHUNK, we need to calculate how many message we
    // need to send
    message_count = (int)round(mics.NumberOfSamples() / CHUNK);

    xEventGroupClearBits(audioGroup, PLAY);
    xEventGroupClearBits(audioGroup, STREAM);
    xEventGroupClearBits(everloopGroup, EVERLOOP);
    xEventGroupClearBits(everloopGroup, ANIMATE);

    // Create task here so led turns red if WiFi does not connect
    xTaskCreatePinnedToCore(everloopTask, "everloopTask", 4096, NULL, 5,
                            &everloopTaskHandle, 1);
    xEventGroupSetBits(everloopGroup, EVERLOOP);

    Serial.begin(115200);
    Serial.println("Booting");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Connection Failed! Rebooting...");
        delay(5000);
        ESP.restart();
    }

    // ---------------------------------------------------------------------------
    // ArduinoOTA
    // ---------------------------------------------------------------------------
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPasswordHash(OTA_PASS_HASH);

    ArduinoOTA
        .onStart([]() {
            // Stop audio processing
            xEventGroupClearBits(audioGroup, STREAM);
            xEventGroupClearBits(audioGroup, PLAY);
            xSemaphoreGive(wbSemaphore);
            isUpdateInProgess = true;
            Serial.println("Uploading...");
            xEventGroupSetBits(everloopGroup, EVERLOOP);
            xTimerStop(wifiReconnectTimer, 0);
            xTimerStop(mqttReconnectTimer, 0);
            asyncClient.disconnect();
            audioServer.disconnect();
        })
        .onEnd([]() {
            isUpdateInProgess = false;
            Serial.println("\nEnd");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR)
                Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR)
                Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR)
                Serial.println("End Failed");
        });
    ArduinoOTA.begin();

    // Create the runnings tasks, AudioStream is on one core, the rest on the
    // other core
    xTaskCreatePinnedToCore(Audiostream, "Audiostream", 10000, NULL, 3,
                            &audioStreamHandle, 0);
    xTaskCreatePinnedToCore(everloopAnimation, "everloopAnimation", 4096, NULL,
                            3, NULL, 1);
    xTaskCreatePinnedToCore(AudioPlayTask, "AudioPlayTask", 4096, NULL, 3,
                            &audioPlayHandle, 1);

    // start streaming
    xEventGroupSetBits(audioGroup, STREAM);
}

/* ************************************************************************ *
      MAIN LOOP
 * ************************************************************************ */
void loop() {
    ArduinoOTA.handle();
    if (!isUpdateInProgess) {
        long now = millis();
        if (!audioServer.connected()) {
            if (now - lastReconnectAudio > 2000) {
                lastReconnectAudio = now;
                // Attempt to reconnect
                if (connectAudio()) {
                    lastReconnectAudio = 0;
                }
            }
        } else {
            audioServer.loop();
        }
        if (now - lastCounterTick > 5000) {
            // reset every 5 seconds. Change leds if there is a problem
            // number of messages should be slightly over 300 per 5 seconds
            lastCounterTick = now;
            if (streamMessageCount < 300) {
                // issue with audiostreaming
                audioOK = false;
                xEventGroupSetBits(everloopGroup, EVERLOOP);
            } else {
                audioOK = true;
                xEventGroupSetBits(everloopGroup, EVERLOOP);
            }
            streamMessageCount = 0;
        }
    }
    delay(1);
}
