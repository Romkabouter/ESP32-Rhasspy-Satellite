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
  virtual void react(IdleEvent const &) {};
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
    xEventGroupClearBits(audioGroup, PLAY);
    xEventGroupClearBits(audioGroup, STREAM);
    device->updateBrightness(config.hotword_brightness);
    if (xSemaphoreTake(wbSemaphore, (TickType_t)10000) == pdTRUE) {
      device->updateColors(COLORS_HOTWORD);
    }
    xEventGroupSetBits(audioGroup, STREAM);
    xSemaphoreGive(wbSemaphore);
    initHeader(device->readSize, device->width, device->rate);
  }

  void react(StreamAudioEvent const &) override { 
    xEventGroupClearBits(audioGroup, PLAY);
    xEventGroupSetBits(audioGroup, STREAM);
  };

  void react(PlayAudioEvent const &) override { 
    xEventGroupClearBits(audioGroup, STREAM);
    xEventGroupSetBits(audioGroup, PLAY);
  };

  void react(IdleEvent const &) override { 
    transit<Idle>();
  }

  void react(WifiDisconnectEvent const &) override { 
    transit<WifiDisconnected>();
  };
};

class Idle : public StateMachine
{
  bool hotwordDetected = false;

  void entry(void) override {
    Serial.println("Enter Idle");
    hotwordDetected = false;
    xEventGroupClearBits(audioGroup, PLAY);
    xEventGroupClearBits(audioGroup, STREAM);
    device->updateBrightness(config.brightness);
    if (xSemaphoreTake(wbSemaphore, (TickType_t)10000) == pdTRUE) {
      device->updateColors(COLORS_IDLE);
    }
    // start streaming audio to the remote endpoint for hotword detection
    if (config.hotword_detection == HW_REMOTE)
    {
      xEventGroupSetBits(audioGroup, STREAM);
    }
    xSemaphoreGive(wbSemaphore);
    initHeader(device->readSize, device->width, device->rate);
  }

  void run(void) override {
    if (device->isHotwordDetected() && !hotwordDetected) {
      hotwordDetected = true;
      //start session by publishing a message to hermes/dialogueManager/startSession
      std::string message = "{\"init\":{\"type\":\"action\",\"canBeEnqueued\": false},\"siteId\":\"" + std::string(SITEID) + "\"}";
      asyncClient.publish("hermes/dialogueManager/startSession", 0, false, message.c_str());
    }
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

  void react(StreamAudioEvent const &) override { 
    xEventGroupClearBits(audioGroup, PLAY);
    xEventGroupSetBits(audioGroup, STREAM);
  };

  void react(PlayAudioEvent const &) override { 
    xEventGroupClearBits(audioGroup, STREAM);
    xEventGroupSetBits(audioGroup, PLAY);
  };

};

class MQTTConnected : public StateMachine {
  void entry(void) override {
    Serial.println("Enter MQTTConnected");
    Serial.printf("Connected as %s\n\r",SITEID);
    publishDebug("Connected to asynch MQTT!");
    asyncClient.subscribe(playBytesTopic.c_str(), 0);
    asyncClient.subscribe(hotwordTopic.c_str(), 0);
    asyncClient.subscribe(audioTopic.c_str(), 0);
    asyncClient.subscribe(debugTopic.c_str(), 0);
    asyncClient.subscribe(ledTopic.c_str(), 0);
    asyncClient.subscribe(restartTopic.c_str(), 0);
    transit<Idle>();
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
    if (!mqttInitialized) {
      asyncClient.onMessage(onMqttMessage);
      mqttInitialized = true;
    }
    asyncClient.setClientId(SITEID);
    asyncClient.setServer(config.mqtt_host, config.mqtt_port);
    asyncClient.setCredentials(MQTT_USER, MQTT_PASS);
    audioServer.setServer(config.mqtt_host, config.mqtt_port);
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
        Serial.println("Connect failed, retry");
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
    xEventGroupClearBits(audioGroup, PLAY);
    xEventGroupClearBits(audioGroup, STREAM);
    device->updateBrightness(config.brightness);
    device->updateColors(COLORS_WIFI_CONNECTED);
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
    //Mute initial output
    device->muteOutput(true);
    xEventGroupClearBits(audioGroup, STREAM);
    xEventGroupClearBits(audioGroup, PLAY);
    xTaskCreatePinnedToCore(I2Stask, "I2Stask", 30000, (void*)device, 1, NULL, 1);
    Serial.println("Enter WifiDisconnected");
    Serial.printf("Total heap: %d\n\r", ESP.getHeapSize());
    Serial.printf("Free heap: %d\n\r", ESP.getFreeHeap());
    device->updateBrightness(config.brightness);
    device->updateColors(COLORS_WIFI_DISCONNECTED);
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

using fsm = tinyfsm::Fsm<StateMachine>;

template<typename E>
void send_event(E const & event)
{
  fsm::template dispatch<E>(event);
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
      StaticJsonDocument<300> doc;
      DeserializationError err = deserializeJson(doc, payloadstr.c_str());
      // Check if this is for us
      if (!err) {
        JsonObject root = doc.as<JsonObject>();
        if (root["siteId"] == SITEID 
          && root.containsKey("reason")
          && root["reason"] == "dialogueSession") {
            send_event(HotwordDetectedEvent());
        }
      }
    } else if (topicstr.find("toggleOn") != std::string::npos) {
      std::string payloadstr(payload);
      StaticJsonDocument<300> doc;
      DeserializationError err = deserializeJson(doc, payloadstr.c_str());
      // Check if this is for us
      if (!err) {
        JsonObject root = doc.as<JsonObject>();
        if (root["siteId"] == SITEID 
          && root.containsKey("reason")
          && root["reason"] == "dialogueSession") {
            send_event(IdleEvent());
          }
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
    } else if (topicstr.find(ledTopic.c_str()) != std::string::npos) {
      std::string payloadstr(payload);
      StaticJsonDocument<300> doc;
      bool saveNeeded = false;
      DeserializationError err = deserializeJson(doc, payloadstr.c_str());
      if (!err) {
        JsonObject root = doc.as<JsonObject>();
        if (root.containsKey("brightness")) {
          if (config.brightness != (int)root["brightness"]) {
            config.brightness = (int)(root["brightness"]);
            saveNeeded = true;
          }
        }
        if (root.containsKey("hotword_brightness")) {
          config.hotword_brightness = (int)(root["hotword_brightness"]);
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
        if (root.containsKey("wifi_connect")) {
          wifi_conn_colors[0] = root["wifi_connect"][0];
          wifi_conn_colors[1] = root["wifi_connect"][1];
          wifi_conn_colors[2] = root["wifi_connect"][2];
          wifi_conn_colors[3] = root["wifi_connect"][3];
        }
        if (root.containsKey("update")) {
          ota_colors[0] = root["update"][0];
          ota_colors[1] = root["update"][1];
          ota_colors[2] = root["update"][2];
          ota_colors[3] = root["update"][3];
        }
        if (saveNeeded) {
          saveConfiguration(configfile, config);
        }
      } else {
        publishDebug(err.c_str());
      }
    } else if (topicstr.find(audioTopic.c_str()) != std::string::npos) {
      std::string payloadstr(payload);
      StaticJsonDocument<300> doc;
      DeserializationError err = deserializeJson(doc, payloadstr.c_str());
      if (!err) {
        JsonObject root = doc.as<JsonObject>();
        if (root.containsKey("mute_input")) {
          config.mute_input = (root["mute_input"] == "true") ? true : false;
        }
        if (root.containsKey("mute_output")) {
          config.mute_output = (root["mute_output"] == "true") ? true : false;
        }
        if (root.containsKey("amp_output")) {
            config.amp_output =  (root["amp_output"] == "0") ? AMP_OUT_SPEAKERS : AMP_OUT_HEADPHONE;
        }
        if (root.containsKey("gain")) {
          config.gain = (int)root["gain"];
        }
        if (root.containsKey("volume")) {
          config.volume = (uint16_t)root["volume"];
        }
        if (root.containsKey("hotword")) {
          config.hotword_detection = (root["hotword"] == "local") ? HW_LOCAL : HW_REMOTE;
        }
        saveConfiguration(configfile, config);
      } else {
          publishDebug(err.c_str());
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
        publishDebug(err.c_str());
      }
    } else if (topicstr.find(debugTopic.c_str()) != std::string::npos) {
      std::string payloadstr(payload);
      StaticJsonDocument<300> doc;
      DeserializationError err = deserializeJson(doc, payloadstr.c_str());
      if (!err) {
        JsonObject root = doc.as<JsonObject>();
        if (root.containsKey("debug")) {
          DEBUG = (root["debug"] == "true") ? true : false;
        }
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
        Serial.printf("Samplerate: %d, Channels: %d, Format: %d, Bits per Sample: %d\n\r", (int)Message.SampleRate, (int)Message.NumChannels, (int)Message.Format, (int)Message.BitsPerSample);
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

void I2Stask(void *p) {  
  while (1) {    
    if (xEventGroupGetBits(audioGroup) == PLAY && xSemaphoreTake(wbSemaphore, (TickType_t)5000) == pdTRUE) {
      size_t bytes_written;
      boolean timeout = false;
      int played = 44;

      device->setWriteMode(sampleRate, bitDepth, numChannels);

      while (played < message_size && timeout == false) {
          int bytes_to_write = device->writeSize;
          if (message_size - played < device->writeSize) {
              bytes_to_write = message_size - played;
          }
          uint8_t data[bytes_to_write];
          if (!timeout) {
            for (int i = 0; i < bytes_to_write; i++) {
              if (!audioData.pop(data[i])) {
                data[i] = 0;
                Serial.println("Buffer underflow");
              }
            }
            played = played + bytes_to_write;
            if (!config.mute_output) {
              device->muteOutput(false);
              device->writeAudio(data, bytes_to_write, &bytes_written);
            } else {
              bytes_written = bytes_to_write;
            }
            if (bytes_written != bytes_to_write) {
              Serial.printf("Bytes to write %d, but bytes written %d\n\r",bytes_to_write,bytes_written);
            }
          }
      }
      asyncClient.publish(playFinishedTopic.c_str(), 0, false, finishedMsg.c_str());
      device->muteOutput(true);
      audioData.clear();
      Serial.println("Done");
      xSemaphoreGive(wbSemaphore); 
      send_event(StreamAudioEvent());
    }
    if (xEventGroupGetBits(audioGroup) == STREAM && !config.mute_input && xSemaphoreTake(wbSemaphore, (TickType_t)5000) == pdTRUE) {     
      device->setReadMode();
      uint8_t data[device->readSize * device->width];
      if (audioServer.connected()) {
        if (device->readAudio(data, device->readSize * device->width)) {
          //Rhasspy needs an audiofeed of 512 bytes+header per message
          //Some devices, like the Matrix Voice do 512 16 bit read in one mic read
          //This is 1024 bytes, so two message are needed in that case
          const int messageBytes = 512;
          uint8_t payload[sizeof(header) + messageBytes];
          const int message_count = sizeof(data) / messageBytes;
          for (int i = 0; i < message_count; i++) {
            memcpy(payload, &header, sizeof(header));
            memcpy(&payload[sizeof(header)], &data[messageBytes * i], messageBytes);
            audioServer.publish(audioFrameTopic.c_str(),(uint8_t *)payload, sizeof(payload));
          }
        } else {
          //Loop, because otherwise this causes timeouts
          audioServer.loop();
        }
      } else {
        xEventGroupClearBits(audioGroup, STREAM);
        send_event(MQTTDisconnectedEvent());
      }
      xSemaphoreGive(wbSemaphore); 
    }
  }  
  vTaskDelete(NULL);
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
