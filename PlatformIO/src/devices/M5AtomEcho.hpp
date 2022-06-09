#pragma once
#include <Arduino.h>
#include <device.h>

#include "M5Atom.h"
#include <driver/i2s.h>

#define CONFIG_I2S_MCK_PIN 0
#define CONFIG_I2S_BCK_PIN 19
#define CONFIG_I2S_LRCK_PIN 33
#define CONFIG_I2S_DATA_PIN 22
#define CONFIG_I2S_DATA_IN_PIN 23

#define SPEAKER_I2S_NUMBER I2S_NUM_0

class M5AtomEcho : public Device
{
public:
  M5AtomEcho();
  void init();
  void animate(StateColors colors, int mode);
  void animateBlinking(StateColors colors);
  void animatePulsing(StateColors colors);
  void updateColors(StateColors colors);
  void updateBrightness(int brightness);
  void setReadMode();
  void setWriteMode(int sampleRate, int bitDepth, int numChannels);
  void writeAudio(uint8_t *data, size_t size, size_t *bytes_written);
  bool readAudio(uint8_t *data, size_t size);
  bool isHotwordDetected();
  int numAmpOutConfigurations() { return 1; };
  bool animationSupported() { return true; };
  bool runningSupported() { return false; };
  bool pulsingSupported() { return true; };
  bool blinkingSupported() { return true; };

private:
  void InitI2SSpeakerOrMic(int mode);
  long currentMillis, startMillis;
  bool ledsOn = true;
  bool directionDown = false;
  int brightness, pulse;
};

M5AtomEcho::M5AtomEcho()
{
};

void M5AtomEcho::init()
{
  M5.begin(true,true,true);
  currentMillis = millis();
  startMillis = millis();
};

bool M5AtomEcho::isHotwordDetected() {
  M5.update();
  return M5.Btn.isPressed();
}

void M5AtomEcho::updateColors(StateColors colors)
{
  //Red and Green seem to be switched, we also need to map the white
  float alpha = 0.6 * (1.0 - (1.0 - ColorMap[colors][3]) * (1.0 - ColorMap[colors][3]));
  int r = (1.0 - alpha) * ColorMap[colors][1] + alpha;
  int g = (1.0 - alpha) * ColorMap[colors][0] + alpha;
  int b = (1.0 - alpha) * ColorMap[colors][2] + alpha;
  M5.dis.drawpix(0, CRGB(r, g, b));
};

void M5AtomEcho::updateBrightness(int brightness) {
  M5.dis.setBrightness(brightness);
  M5AtomEcho::pulse = brightness;
  M5AtomEcho::brightness = 0;
}

void M5AtomEcho::animate(StateColors colors, int mode) {
  switch (mode)
  {
  case AnimationMode::BLINK:
    animateBlinking(colors);
    break;
  case AnimationMode::PULSE:
    animatePulsing(colors);
    break;
  default:
    break;
  }
}

void M5AtomEcho::animatePulsing(StateColors colors) {
  currentMillis = millis();
  if (currentMillis - startMillis > 5) {
    if (M5AtomEcho::pulse > M5AtomEcho::brightness) { directionDown = true; }
    M5AtomEcho::pulse = directionDown ? M5AtomEcho::pulse - 5 : M5AtomEcho::pulse + 5;
    if (M5AtomEcho::pulse < 5) { directionDown = false; }
    startMillis = millis();
    M5.dis.setBrightness(M5AtomEcho::pulse);
    updateColors(colors);
  }
}

void M5AtomEcho::animateBlinking(StateColors colors) {
  currentMillis = millis();
  if (currentMillis - startMillis > 300) {
    M5.dis.setBrightness(ledsOn ? M5AtomEcho::brightness : 0);
    ledsOn = !ledsOn;
    startMillis = millis();
    updateColors(colors);
  }
}

void M5AtomEcho::InitI2SSpeakerOrMic(int mode)
{
    esp_err_t err = ESP_OK;

    i2s_driver_uninstall(SPEAKER_I2S_NUMBER);
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER),
        .sample_rate = 16000,
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

    tx_pin_config.mck_io_num = CONFIG_I2S_MCK_PIN;
    tx_pin_config.bck_io_num = CONFIG_I2S_BCK_PIN;
    tx_pin_config.ws_io_num = CONFIG_I2S_LRCK_PIN;
    tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
    tx_pin_config.data_in_num = CONFIG_I2S_DATA_IN_PIN;

    err += i2s_set_pin(SPEAKER_I2S_NUMBER, &tx_pin_config);
    err += i2s_set_clk(SPEAKER_I2S_NUMBER, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

    return;
}

void M5AtomEcho::setWriteMode(int sampleRate, int bitDepth, int numChannels) {
  if (mode != MODE_SPK) {
    M5AtomEcho::InitI2SSpeakerOrMic(MODE_SPK);
    mode = MODE_SPK;
  }
  if (sampleRate > 0) {
    i2s_set_clk(SPEAKER_I2S_NUMBER, sampleRate, static_cast<i2s_bits_per_sample_t>(bitDepth), static_cast<i2s_channel_t>(numChannels));
  }
}

void M5AtomEcho::setReadMode() {
  if (mode != MODE_MIC) {
    M5AtomEcho::InitI2SSpeakerOrMic(MODE_MIC);
    mode = MODE_MIC;
  }
}

void M5AtomEcho::writeAudio(uint8_t *data, size_t size, size_t *bytes_written) {
  i2s_write(SPEAKER_I2S_NUMBER, data, size, bytes_written, portMAX_DELAY);
}

bool M5AtomEcho::readAudio(uint8_t *data, size_t size) {
  size_t byte_read;
  i2s_read(SPEAKER_I2S_NUMBER, data, size, &byte_read, (100 / portTICK_RATE_MS));
  return true;
}