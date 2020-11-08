/* ************************************************************************* *
   Matrix Voice Audio Streamer

   This program is written to be a streaming audio server running on the Matrix
   Voice. This is typically used for Snips.AI or Rhasspy, it will then be able to replace the
   Snips Audio Server, by publishing small wave messages to the hermes protocol
   See https://snips.ai/ or https://rhasspy.readthedocs.io/en/latest/ for more information

   Author:  Paul Romkes
   Date:    October 2020
   Version: 7.0

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
    - Add dynamic brightness, post {"brightness": 50 } to SITEID/everloop
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
    - playBytes working, only plays 44100 samplerate (mono/stereo) correctly. Work in progress
    - Upgrade to ArduinoJSON 6
    - Add mute/unmute via MQTT
    - Fixed OTA issues, remove webserver
   v4.1:
    - Configurable mic gain
    - Fix on only listening to Dutch Rhasspy
   v4.2:
    - Support platformIO
    v4.3:
    - Force platform 1.9.0. Higher raises issues with the mic array
    - Add muting of output and switching of output port
   v4.4:
    - Fix distortion issues, caused by incorrect handling of incoming audio
    - Added resampling using Speex, resamples 8000 and up and converts mono 
      to stereo. 
   v4.5:
    - Support streaming audio
   v4.5.1:
    - Fix distortion on lower samplerates
   v5.0:
    - Added ondevice wakeword detection using WakeNet, only Alexa available
   v5.1:
    - Added volume control, publish {"volume": 50} to the sitesid/audio topic
   v5.12:
    - Add dynamic hotword brightness, post {"hotword_brightness": 50 } to SITEID/everloop
   v5.12.1:
    - Fixed a couple of defects regarding input mute and disconnects
   v6.0:
    - Added configuration webserver
    - Improved stability for MQTT stream
   v7.0:
    - Complete rewrite
* ************************************************************************ */

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
// extern "C" {
//     #include "freertos/FreeRTOS.h"
//     #include "freertos/event_groups.h"
//     #include "freertos/timers.h"
// }
#include "Application.h"

bool isUpdateInProgess = false;
bool wifi_connected = false;
TimerHandle_t wifiReconnectTimer;
TaskHandle_t applicationTaskHandle;
int retryCount = 0;

void connectToWifi() {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    retryCount = 0;
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        retryCount++;
        if (retryCount > 2) {
            Serial.println("Connection Failed! Rebooting...");
            ESP.restart();
        }
    }
}

void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_STA_START:
            WiFi.setHostname(HOSTNAME);
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            wifi_connected = true;
            //xTaskNotify(applicationTaskHandle, 1, eSetBits);
            Serial.println("Connected to Wifi with IP: " + WiFi.localIP().toString());
            xTimerStop(wifiReconnectTimer, 0);  // Stop the reconnect timer
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            wifi_connected = false;
            Serial.println("Disconnected from Wifi!");
            xTimerStart(wifiReconnectTimer, 0);  // Start the reconnect timer
            break;
        default:
            break;
    }
}

void applicationTask(void *param)
{
  Application *application = static_cast<Application *>(param);

 // const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
//  uint32_t ulNotificationValue = 1;
  while (true)
  {
    //uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    
//    if (ulNotificationValue > 0)
//    {
      application->run();
//      ulNotificationValue = 0;
//    }
  }
}

/* ************************************************************************ *
      SETUP
 * ************************************************************************ */
void setup() {
    Serial.begin(115200);
    Serial.println("Booting");
    //Application *application = new Application();

    wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdTRUE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

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

  //  xTaskCreatePinnedToCore(applicationTask, "Application Task", 8192, application, 1, &applicationTaskHandle, 0);
    //xTaskNotify(applicationTaskHandle, 1, eIncrement);

    // ---------------------------------------------------------------------------
    // ArduinoOTA
    // ---------------------------------------------------------------------------
    ArduinoOTA.setPasswordHash(OTA_PASS_HASH);

    ArduinoOTA
        .onStart([]() {
        //    vTaskSuspend(applicationTaskHandle);
            isUpdateInProgess = true;
            Serial.println("Uploading...");
            xTimerStop(wifiReconnectTimer, 0);
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
}

/* ************************************************************************ *
      MAIN LOOP
 * ************************************************************************ */
void loop() {
    ArduinoOTA.handle();
}
