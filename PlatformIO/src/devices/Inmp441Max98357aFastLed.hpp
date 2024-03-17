#pragma once
#include <Arduino.h>
#include <device.h>

#include "IndicatorLight.h"
#include <driver/i2s.h>

#include <FastLED.h>

// Led strip
#define LED_PIN 26
#define NUM_LEDS 5
CRGB leds[NUM_LEDS];

// I2S pins & parameters for MIC
#define MIC_I2S_BCLK 25
#define MIC_I2S_FS 32
#define MIC_I2S_DIN 33
#define MIC_I2S_PORT I2S_NUM_0
#define MIC_SAMPLE_RATE (16000)
#define MIC_I2S_SAMPLE_BITS 32
#define MIC_I2S_SAMPLE_BYTES (MIC_I2S_SAMPLE_BITS / 8)
#define I2S_READ_LEN (256 * MIC_I2S_SAMPLE_BYTES)

// I2S pins & parameters for speaker
#define SPK_I2S_FS 12
#define SPK_I2S_BCLK 14
#define SPK_I2S_DOUT 27
#define SPK_I2S_PORT I2S_NUM_1
#define SPK_SAMPLE_RATE (22050)
#define SPK_I2S_SAMPLE_BITS 16
#define SPK_I2S_SAMPLE_BYTES (SPK_I2S_SAMPLE_BITS / 8)

class Inmp441Max98357aFastLED : public Device
{
public:
  Inmp441Max98357aFastLED() = default;

  void init();
  bool readAudio(uint8_t *data, size_t size);

  void setWriteMode(int sampleRate, int bitDepth, int numChannels);
  void writeAudio(uint8_t *data, size_t size, size_t *bytes_written);
  void setGain(uint16_t gain);

  int numAmpOutConfigurations()
  {
    return 1;
  }

  void updateColors(StateColors colors);
  void updateBrightness(int brightness);

  bool animationSupported()
  {
    return true;
  }
  bool runningSupported()
  {
    return true;
  }
  bool blinkingSupported()
  {
    return true;
  }

  void animate(StateColors colors, int mode);
  void animateRunning(StateColors colors);
  void animateBlinking(StateColors colors);

private:
  char *i2s_read_buff = (char *)calloc(I2S_READ_LEN, sizeof(char));
  uint16_t m_gain;
  long currentMillis, startMillis;
};

void Inmp441Max98357aFastLED::init()
{
  esp_err_t err = ESP_OK;
  Serial.printf("Connect to Inmp441... \n");

  // Speakers
  i2s_config_t i2sConfig_tx = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = SPK_SAMPLE_RATE,                                                   // I2S_SAMPLE_RATE,
      .bits_per_sample = static_cast<i2s_bits_per_sample_t>(I2S_BITS_PER_SAMPLE_16BIT), // I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 2,
      .dma_buf_len = 512,
      .tx_desc_auto_clear = true,
  };

  i2s_pin_config_t pin_config_tx = {
      .bck_io_num = SPK_I2S_BCLK, .ws_io_num = SPK_I2S_FS, .data_out_num = SPK_I2S_DOUT, .data_in_num = -1};

  err += i2s_driver_install(SPK_I2S_PORT, &i2sConfig_tx, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing headphone driver: %d\n", err);
    while (true)
      ;
  }

  err += i2s_set_pin(SPK_I2S_PORT, &pin_config_tx);
  if (err != ESP_OK) {
    Serial.printf("Failed setting headphone pin: %d\n", err);
    while (true)
      ;
  }
  Serial.println("I2S headphone driver installed.\n");

  //----- Microphone
  /*
    - the INMP441 MEMS microphone has a specified sensitivity of -26 dBFS @ 1 Pa,
      or 120 dB SPL @ FS.
    - Even a loud speech signal won't exceed 80 dB, so we "waste" the top 40 dB
      of dynamic range.
    - The microphone sends 24 bit samples (which is not to say it has a true
      24 bit dynamic range).
    - If we read 16 bit samples (nominal 96 dB dynamic range), and waste the top
      40 dB, then we get a ~56 dB dynamic range speech signal.
    - Might bet better to read 24 bit samples, and amplify by 40 dB ?
    - in my experiments, a x20 gain (26 dB) will result in a 50% FS signal, i.e.
      not clipping, with a loud voice at 30cm distance
  */
  i2s_config_t i2s_config = {.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
                             .sample_rate = MIC_SAMPLE_RATE,
                             .bits_per_sample = static_cast<i2s_bits_per_sample_t>(MIC_I2S_SAMPLE_BITS),
                             .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
                             .communication_format = I2S_COMM_FORMAT_STAND_I2S,
                             .intr_alloc_flags = 0,
                             .dma_buf_count = 2,
                             .dma_buf_len = I2S_READ_LEN,
                             .use_apll = 1};

  i2s_pin_config_t pin_config = {
      .bck_io_num = MIC_I2S_BCLK, .ws_io_num = MIC_I2S_FS, .data_out_num = -1, .data_in_num = MIC_I2S_DIN};

  err += i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing mic driver: %d\n", err);
    while (true)
      ;
  }

  err += i2s_set_pin(MIC_I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting mic pin: %d\n", err);
    while (true)
      ;
  }
  Serial.println("I2S driver mic installed.");

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

  return;
}

void Inmp441Max98357aFastLED::updateColors(StateColors colors)
{
  if (colors == StateColors::COLORS_IDLE) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
  } else {
    fill_solid(leds, NUM_LEDS, CRGB(ColorMap[colors][0], ColorMap[colors][1], ColorMap[colors][2]));
    FastLED.show();
  }
}

void Inmp441Max98357aFastLED::animate(StateColors colors, int mode)
{
  switch (mode) {
  case AnimationMode::RUN:
    animateRunning(colors);
    break;
  case AnimationMode::BLINK:
    animateBlinking(colors);
    break;
  default:
    break;
  }
}

void Inmp441Max98357aFastLED::animateRunning(StateColors colors)
{
  static uint8_t current_position = 0;
  currentMillis = millis();
  if (currentMillis - startMillis > 50) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    leds[current_position] = CRGB(ColorMap[colors][0], ColorMap[colors][1], ColorMap[colors][2]);
    FastLED.show();
    ++current_position %= NUM_LEDS;
    startMillis = millis();
  }
}

void Inmp441Max98357aFastLED::animateBlinking(StateColors colors)
{
  static bool currently_on = false;
  currentMillis = millis();
  if (currentMillis - startMillis > 300) {
    if (currently_on) {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
    } else {
      fill_solid(leds, NUM_LEDS, CRGB(ColorMap[colors][0], ColorMap[colors][1], ColorMap[colors][2]));
    }
    FastLED.show();
    currently_on = !currently_on;
    startMillis = millis();
  }
}

void Inmp441Max98357aFastLED::setWriteMode(int sampleRate, int bitDepth, int numChannels)
{
  if (sampleRate > 0) {
    i2s_set_clk(SPK_I2S_PORT, sampleRate, static_cast<i2s_bits_per_sample_t>(bitDepth),
                static_cast<i2s_channel_t>(numChannels));
  }
}

void Inmp441Max98357aFastLED::writeAudio(uint8_t *data, size_t size, size_t *bytes_written)
{
  i2s_write(SPK_I2S_PORT, data, size, bytes_written, portMAX_DELAY);
}

bool Inmp441Max98357aFastLED::readAudio(uint8_t *data, size_t size)
{
  size_t samples_requested = size / sizeof(int16_t);
  size_t bytes_requested = MIC_I2S_SAMPLE_BYTES * samples_requested;
  size_t bytes_read;
  // we skip every other sample, so read 2x desired # of samples
  i2s_read(MIC_I2S_PORT, (void *)i2s_read_buff, bytes_requested, &bytes_read, portMAX_DELAY);

  int16_t *from = (int16_t *)i2s_read_buff;
  int16_t *to = (int16_t *)data;
  size_t nsamples = bytes_read / MIC_I2S_SAMPLE_BYTES;
  size_t i;
  int32_t y;
  /*
    according to https://www.esp32.com/viewtopic.php?t=11023 , incoming samples
    are in flipped order, i.e. 1,0,3,2,5,4,... instead of 0,1,2,3,4,5,...
    As of ESP-IF 4, this is not true anymore. However, reading from the
    microphone as int16_t appears to repeat every sample twice, so we read
    as int32_t instead, and ignore the 16 LSBs
  */
  for (i = 0; i < nsamples; i++) {
    y = from[1]; // read 16 MSB of the 32 bit value
    y *= m_gain;
    *to++ = y;
    from += 2;
  }
  return true;
}

void Inmp441Max98357aFastLED::updateBrightness(int brightness)
{
  FastLED.setBrightness(brightness);
}

void Inmp441Max98357aFastLED::setGain(uint16_t gain)
{
  m_gain = gain;
}