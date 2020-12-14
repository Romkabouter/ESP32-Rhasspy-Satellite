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
    - Complete rewrite using StateMachine
* ************************************************************************ */

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <StateMachine.hpp>
#include "M5Atom.h"

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  M5.begin(true,true,true);
  // ---------------------------------------------------------------------------
  // ArduinoOTA
  // ---------------------------------------------------------------------------
  ArduinoOTA.setPasswordHash(OTA_PASS_HASH);

  ArduinoOTA
    .onStart([]() {
      Serial.println("Uploading...");
    })
    .onEnd([]() {
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
 
  fsm_list::start();
}

void loop() {
  if (WiFi.isConnected()) {
    ArduinoOTA.handle();
  }
  fsm_list::run();
}
