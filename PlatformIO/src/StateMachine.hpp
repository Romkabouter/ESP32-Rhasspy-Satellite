#include <tinyfsm.hpp>
#include <AsyncMqttClient.h>
#include <PubSubClient.h>
#include "RingBuf.h"

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
    device->updateLeds(COLORS_HOTWORD);
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
    device->updateLeds(COLORS_STREAM);
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

  private:
  long currentMillis, startMillis;

  void entry(void) override {
    Serial.println("Enter MQTTDisconnected");
    startMillis = millis();
    currentMillis = millis();
    if (audioServer.connected()) {
      audioServer.disconnect();
    }    
    if (asyncClient.connected()) {
      asyncClient.disconnect();
    }
    asyncClient.setClientId(SITEID);
    asyncClient.setServer(MQTT_HOST, MQTT_PORT);
    asyncClient.setCredentials(MQTT_USER, MQTT_PASS);
    asyncClient.onMessage(onMqttMessage);
    audioServer.setServer(MQTT_HOST, MQTT_PORT);
    char clientID[100];
    sprintf(clientID, "%sAudio", SITEID);
    asyncClient.connect();
    audioServer.connect(clientID, MQTT_USER, MQTT_PASS);
  }

  void run(void) override {
    if (audioServer.connected() && asyncClient.connected()) {
      transit<MQTTConnected>();
    } else {
      currentMillis = millis();
      if (currentMillis - startMillis > 5000) {
        Serial.printf("Connect failed after %d, retry\n",(int)currentMillis - startMillis);
        transit<MQTTDisconnected>();
      }      
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
    device->updateLeds(COLORS_WIFI_CONNECTED);
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
    device->updateLeds(COLORS_WIFI_DISCONNECTED);
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
        while (audioData.isFull()) {
          if (xEventGroupGetBits(audioGroup) != PLAY) {
            send_event(PlayAudioEvent());
          }
          vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        audioData.push((uint8_t)payload[i]);
      }
      //At the end, make sure to start play incase the buffer is not full yet
      if (!audioData.isEmpty() && xEventGroupGetBits(audioGroup) != PLAY) {
        send_event(PlayAudioEvent());
      }
    }
  } else {
    // len + index < total ==> partial message
    if (topicstr.find("playBytes") != std::string::npos) {
      if (index == 0) {
        message_size = total;
        audioData.clear();
        for (int i = 0; i < 44; i++) {
          audioData.push((uint8_t)payload[i]);
        }
        for (int k = 0; k < 44; k++) {
            audioData.pop(WaveData[k]);
        }
        XT_Wav_Class Message((const uint8_t *)WaveData);
        Serial.printf("Samplerate: %d, Channels: %d, Format: %d, Bits per Sample: %d\n", (int)Message.SampleRate, (int)Message.NumChannels, (int)Message.Format, (int)Message.BitsPerSample);
        sampleRate = (int)Message.SampleRate;
        numChannels = (int)Message.NumChannels;
        bitDepth = (int)Message.BitsPerSample;
        queueDelay = ((int)Message.SampleRate * (int)Message.NumChannels * (int)Message.BitsPerSample) /  1000;
        //delay *= 2;
        //Serial.printf("Delay %d\n", (int)queueDelay);
        for (int i = 44; i < len; i++) {
          while (audioData.isFull()) {
            if (xEventGroupGetBits(audioGroup) != PLAY) {
              Serial.println("PlayAudioEvent");
              send_event(PlayAudioEvent());
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
          }
          audioData.push((uint8_t)payload[i]);
        }
      } else {
        for (int i = 0; i < len; i++) {
          while (audioData.isFull()) {
            if (xEventGroupGetBits(audioGroup) != PLAY) {
              send_event(PlayAudioEvent());
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
          }
          audioData.push((uint8_t)payload[i]);
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
    if (xEventGroupGetBits(audioGroup) == PLAY) {
      size_t bytes_written;
      boolean timeout = false;
      int played = 44;

      device->setWriteMode(sampleRate, bitDepth, numChannels);

      while (played < message_size && timeout == false) {
          int bytes_to_read = WRITE_SIZE;
          if (message_size - played < WRITE_SIZE) {
              bytes_to_read = message_size - played;
          }
          uint8_t data[bytes_to_read];
          if (!timeout) {
            for (int i = 0; i < bytes_to_read; i++) {
              if (!audioData.pop(data[i])) {
                data[i] = 0;
                Serial.println("Buffer underflow");
              }
            }
            played = played + bytes_to_read;
            device->writeAudio(data, bytes_to_read, &bytes_written);
            if (bytes_written != bytes_to_read) {
              Serial.printf("Bytes to write %d, but bytes written %d\n",bytes_to_read,bytes_written);
            }
          }
      }
      audioData.clear();
      xEventGroupClearBits(audioGroup, PLAY);
      send_event(StreamAudioEvent());
    } else if (xEventGroupGetBits(audioGroup) == STREAM) {
      device->setReadMode();
      uint8_t data[READ_SIZE * WIDTH];
      if (audioServer.connected()) {
        device->readAudio(data, READ_SIZE * WIDTH);
        uint8_t payload[sizeof(header) + (READ_SIZE * WIDTH)];
        memcpy(payload, &header, sizeof(header));
        memcpy(&payload[sizeof(header)], data,sizeof(data));
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
