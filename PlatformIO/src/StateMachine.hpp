#include <tinyfsm.hpp>
#include "M5Atom.h"
#include <driver/i2s.h>
#include <AsyncMqttClient.h>
#include <PubSubClient.h>
#include "RingBuf.h"

#define CONFIG_I2S_BCK_PIN 19
#define CONFIG_I2S_LRCK_PIN 33
#define CONFIG_I2S_DATA_PIN 22
#define CONFIG_I2S_DATA_IN_PIN 23

#define SPEAKER_I2S_NUMBER I2S_NUM_0

#define MODE_MIC 0
#define MODE_SPK 1
#define READ_SIZE 256
#define WRITE_SIZE 256
#define WIDTH 2
#define RATE 16000

const int PLAY = BIT0;
const int STREAM = BIT1;

uint8_t micdata[READ_SIZE * WIDTH];
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
std::string audioFrameTopic = std::string("hermes/audioServer/") + SITEID + std::string("/audioFrame");
std::string playBytesTopic = std::string("hermes/audioServer/") + SITEID + std::string("/playBytes/#");
std::string hotwordTopic = "hermes/hotword/#";
std::string debugTopic = SITEID + std::string("/debug");
AsyncMqttClient asyncClient; 
WiFiClient net;
PubSubClient audioServer(net); 
RingBuf<uint8_t, 1024 * 6> audioData;
long message_size = 0;
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
void I2Stask(void *p);

class StateMachine
: public tinyfsm::Fsm<StateMachine>
{
public:
  virtual void react(WifiDisconnectEvent const &) {};
  virtual void react(WifiConnectEvent const &) {};
  virtual void react(MQTTConnectedEvent const &) {};
  virtual void react(MQTTDisconnectedEvent const &) {};
  virtual void react(StreamAudioEvent const &) {};
  virtual void react(PlayAudioEvent const &) {};
  virtual void react(HotwordDetectedEvent const &) {};

  virtual void entry(void) {}; 
  virtual void run(void) {}; 
  void         exit(void) {};
};

class HotwordDetected : public StateMachine
{
  void entry(void) override {
    Serial.println("Enter HotwordDetected");
    M5.dis.drawpix(0, CRGB(0xFF0000));
    xEventGroupClearBits(audioGroup, PLAY);
    xEventGroupSetBits(audioGroup, STREAM);
    initHeader();
  }

  void react(StreamAudioEvent const &) override { 
    transit<StreamAudio>();
  };

  void react(WifiDisconnectEvent const &) override { 
    transit<WifiDisconnected>();
  };

  void react(PlayAudioEvent const &) override { 
    transit<PlayAudio>();
  };
};

class PlayAudio : public StateMachine
{
  void entry(void) override {
    Serial.println("Enter PlayAudio");
    //Set the taskbits to play the audiobuffer
    xEventGroupClearBits(audioGroup, STREAM);
    xEventGroupSetBits(audioGroup, PLAY);
  }

  void react(WifiDisconnectEvent const &) override { 
    transit<WifiDisconnected>();
  };

  void react(StreamAudioEvent const &) override { 
    transit<StreamAudio>();
  };
};

class StreamAudio : public StateMachine
{
  void entry(void) override {
    Serial.println("Enter StreamAudio");
    M5.dis.drawpix(0, CRGB(0x0000FF)); 
    xEventGroupClearBits(audioGroup, PLAY);
    xEventGroupSetBits(audioGroup, STREAM);
    initHeader();
  }

  void react(WifiDisconnectEvent const &) override { 
    transit<WifiDisconnected>();
  }

  void react(MQTTDisconnectedEvent const &) override { 
    transit<MQTTDisconnected>();
  }

  void react(HotwordDetectedEvent const &) override { 
    transit<HotwordDetected>();
  }

  void react(PlayAudioEvent const &) override { 
    transit<PlayAudio>();
  };

};

class MQTTConnected : public StateMachine {
  void entry(void) override {
    Serial.println("Enter MQTTConnected");
    Serial.printf("Connected as %s\n",SITEID);
    asyncClient.subscribe(playBytesTopic.c_str(), 0);
    asyncClient.subscribe(hotwordTopic.c_str(), 0);
    transit<StreamAudio>();
  }

  void react(MQTTDisconnectedEvent const &) override { 
    transit<MQTTDisconnected>();
  }

  void react(WifiDisconnectEvent const &) override { 
    transit<WifiDisconnected>();
  }
};

class MQTTDisconnected : public StateMachine {
  void entry(void) override {
    Serial.println("Enter MQTTDisconnected");
    if (audioServer.connected()) {
      audioServer.disconnect();
    }    
    if (asyncClient.connected()) {
      asyncClient.disconnect();
    }
    asyncClient.setClientId(SITEID);
    asyncClient.setServer(MQTT_HOST, MQTT_PORT);
    asyncClient.setCredentials(MQTT_USER, MQTT_PASS);
    asyncClient.connect();
    asyncClient.onDisconnect(onMqttDisconnect);
    asyncClient.onMessage(onMqttMessage);
    audioServer.setServer(MQTT_HOST, MQTT_PORT);
    audioServer.connect("MatrixVoiceAudio", MQTT_USER, MQTT_PASS);
  }

  void run(void) override {
    if (audioServer.connected() && asyncClient.connected()) {
      transit<MQTTConnected>();
    }
  }

  void react(MQTTConnectedEvent const &) override { 
    transit<MQTTConnected>();
  }

  void react(WifiDisconnectEvent const &) override { 
    transit<WifiDisconnected>();
  }
};

class WifiConnected : public StateMachine
{
  void entry(void) override {
    Serial.println("Enter WifiConnected");
    Serial.println("Connected to Wifi with IP: " + WiFi.localIP().toString());
    M5.dis.drawpix(0, CRGB(0x0000FF));
    ArduinoOTA.begin();
    transit<MQTTDisconnected>();
  }

  void react(WifiDisconnectEvent const &) override { 
    Serial.println("DisconnectEvent");
    transit<WifiDisconnected>();
  };
};

class WifiDisconnected : public StateMachine
{
  void entry(void) override {
    if (!audioGroup) {
      audioGroup = xEventGroupCreate();
    }
    xEventGroupClearBits(audioGroup, STREAM);
    xEventGroupClearBits(audioGroup, PLAY);
    xTaskCreatePinnedToCore(I2Stask, "I2Stask", 30000, NULL, 1, NULL, 1);
    Serial.println("Enter WifiDisconnected");
    Serial.printf("Total heap: %d\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());    
    M5.dis.drawpix(0, CRGB(0x00FF00));
    WiFi.onEvent(WiFiEvent);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        retryCount++;
        if (retryCount > 2) {
            Serial.println("Connection Failed! Rebooting...");
            ESP.restart();
        } else {
            Serial.println("Connection Failed! Retry...");
        }
    }
  }

  void react(WifiConnectEvent const &) override { 
    transit<WifiConnected>();
  };
};

FSM_INITIAL_STATE(StateMachine, WifiDisconnected)

using fsm_list = tinyfsm::FsmList<StateMachine>;

template<typename E>
void send_event(E const & event)
{
  fsm_list::template dispatch<E>(event);
}

std::vector<std::string> explode( const std::string &delimiter, const std::string &str)
{
    std::vector<std::string> arr;
 
    int strleng = str.length();
    int delleng = delimiter.length();
    if (delleng==0)
        return arr;//no change
 
    int i=0;
    int k=0;
    while( i<strleng )
    {
        int j=0;
        while (i+j<strleng && j<delleng && str[i+j]==delimiter[j])
            j++;
        if (j==delleng)//found delimiter
        {
            arr.push_back(  str.substr(k, i-k) );
            i+=delleng;
            k=i;
        }
        else
        {
            i++;
        }
    }
    arr.push_back(  str.substr(k, i-k) );
    return arr;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  send_event(MQTTDisconnectedEvent());
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  std::string topicstr(topic);
  if (len + index == total) {
    if (topicstr.find("toggleOff") != std::string::npos) {
        std::string payloadstr(payload);
        // Check if this is for us
        if (payloadstr.find(SITEID) != std::string::npos) {
          send_event(HotwordDetectedEvent());
        }
    } else if (topicstr.find("toggleOn") != std::string::npos) {
        // Check if this is for us
        std::string payloadstr(payload);
        if (payloadstr.find(SITEID) != std::string::npos) {  
          send_event(StreamAudioEvent());
        }
    } else if (topicstr.find("playBytes") != std::string::npos) {
      std::vector<std::string> topicparts = explode("/", topicstr);
      finishedMsg = "{\"id\":\"" + topicparts[4] + "\",\"siteId\":\"" + SITEID + "\",\"sessionId\":null}";
      for (int i = 0; i < len; i++) {
        while (!audioData.push((uint8_t)payload[i])) {
          delay(1);
        }
        if (audioData.isFull()) {
          send_event(PlayAudioEvent());
        }
      }
    }
  } else {
    // len + index < total ==> partial message
    if (topicstr.find("playBytes") != std::string::npos) {
      if (index == 0) {
        message_size = total;
        for (int i = 0; i < len; i++) {
          audioData.push((uint8_t)payload[i]);
          if (audioData.isFull()) {
            send_event(PlayAudioEvent());
          }
        }
      } else {
        for (int i = 0; i < len; i++) {
          while (!audioData.push((uint8_t)payload[i])) {
              delay(1);
          }
          if (audioData.isFull()) {
            send_event(PlayAudioEvent());
          }
        }
      }
    }
  }
}

void publishDebug(const char* message) {
  if (true) {
    audioServer.publish(debugTopic.c_str(), message);
  }
}

void I2Stask(void *p) {
  while (1) {
    if (xEventGroupGetBitsFromISR(audioGroup) == PLAY) {
      if (I2SMode != MODE_SPK) {
        InitI2SSpeakerOrMic(MODE_SPK);
        I2SMode = MODE_SPK;
      }
      size_t bytes_written;
      boolean timeout = false;
      int played = 0;
      long now = millis();
      long lastBytesPlayed = millis();
      uint8_t WaveData[44];
      for (int k = 0; k < 44; k++) {
          audioData.pop(WaveData[k]);
          played++;
      }
      while (played < message_size && timeout == false) {
          int bytes_to_read = WRITE_SIZE;
          if (message_size - played < WRITE_SIZE) {
              bytes_to_read = message_size - played;
          }
          uint8_t data[bytes_to_read];
          while (audioData.size() < bytes_to_read && played < message_size && timeout == false) {
              vTaskDelay(1);
              now = millis();
              if (now - lastBytesPlayed > 1000) {
                  //force exit
                Serial.printf("Exit timeout, audioData.size : %d, bytes_to_read: %d, played: %d, message_size: %d\n",(int)audioData.size(), (int)bytes_to_read, (int)played, (int)message_size);
                timeout = true;
              }
          }
          lastBytesPlayed = millis();

          if (!timeout) {
            for (int i = 0; i < bytes_to_read; i++) {
                audioData.pop(data[i]);
            }
            played = played + bytes_to_read;
            i2s_write(SPEAKER_I2S_NUMBER, data, sizeof(data), &bytes_written, portMAX_DELAY);
            //if (bytes_written < bytes_to_read) {
              Serial.printf("Bytes written %d\n",bytes_written);
            //}
          }
      }
      audioData.clear();
      xEventGroupClearBits(audioGroup, PLAY);
      send_event(StreamAudioEvent());
    } else if (xEventGroupGetBitsFromISR(audioGroup) == STREAM) {
      if (I2SMode != MODE_MIC) {
        InitI2SSpeakerOrMic(MODE_MIC);
        I2SMode = MODE_MIC;
      }
      size_t byte_read;
      if (audioServer.connected()) {
        i2s_read(SPEAKER_I2S_NUMBER, (char *)(micdata), READ_SIZE * WIDTH, &byte_read, (100 / portTICK_RATE_MS));
        uint8_t payload[sizeof(header) + (READ_SIZE * WIDTH)];
        memcpy(payload, &header, sizeof(header));
        memcpy(&payload[sizeof(header)], micdata,sizeof(micdata));
        audioServer.publish(audioFrameTopic.c_str(),(uint8_t *)payload, sizeof(payload));
      } else {
        xEventGroupClearBits(audioGroup, STREAM);
        send_event(MQTTDisconnectedEvent());
      }
    }
  }  
  vTaskDelete(NULL);
}

void initHeader() {
    strncpy(header.riff_tag, "RIFF", 4);
    strncpy(header.wave_tag, "WAVE", 4);
    strncpy(header.fmt_tag, "fmt ", 4);
    strncpy(header.data_tag, "data", 4);

    header.riff_length = (uint32_t)sizeof(header) + (READ_SIZE * WIDTH);
    header.fmt_length = 16;
    header.audio_format = 1;
    header.num_channels = 1;
    header.sample_rate = RATE;
    header.byte_rate = RATE * WIDTH;
    header.block_align = WIDTH;
    header.bits_per_sample = WIDTH * 8;
    header.data_length = READ_SIZE * WIDTH;
}

void InitI2SSpeakerOrMic(int mode)
{
    esp_err_t err = ESP_OK;

    i2s_driver_uninstall(SPEAKER_I2S_NUMBER);
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER),
        .sample_rate = RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
        .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 6,
        .dma_buf_len = 60,
    };
    if (mode == MODE_MIC)
    {
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    }
    else
    {
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        i2s_config.use_apll = false;
        i2s_config.tx_desc_auto_clear = true;
    }

    err += i2s_driver_install(SPEAKER_I2S_NUMBER, &i2s_config, 0, NULL);
    i2s_pin_config_t tx_pin_config;

    tx_pin_config.bck_io_num = CONFIG_I2S_BCK_PIN;
    tx_pin_config.ws_io_num = CONFIG_I2S_LRCK_PIN;
    tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
    tx_pin_config.data_in_num = CONFIG_I2S_DATA_IN_PIN;

    err += i2s_set_pin(SPEAKER_I2S_NUMBER, &tx_pin_config);
    err += i2s_set_clk(SPEAKER_I2S_NUMBER, RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

    return;
}

void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_STA_START:
            WiFi.setHostname(HOSTNAME);
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            send_event(WifiConnectEvent());
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            send_event(WifiDisconnectEvent());
            break;
        default:
            break;
    }
}
