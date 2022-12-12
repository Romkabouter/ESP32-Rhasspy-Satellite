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

// I2S pins on ESp32cam MIC
// GPIO2 <--> WS
// GPIO14 <--> SCK
// GPIO15 <--> SD
#define I2S_SCK 25
#define I2S_WS 32
#define I2S_SD 33
#define I2S_PORT I2S_NUM_0

// I2S pins on ESp32Cam Speakers
// GPIO16 <--> LRC
// GPIO13 <--> BCLK
// GPIO12 <--> DIN
#define I2S_LRC 12
#define I2S_BCLK 14
#define I2S_DIN 27

#define I2S_PORT_TX I2S_NUM_1

#define I2S_OUTPUT_SAMPLE_RATE (22050)
#define I2S_SAMPLE_RATE (16000)
#define I2S_SAMPLE_BITS (16)
#define I2S_READ_LEN 512
// LEDs
#define LED_FLASH 4

#define KEY1_GPIO GPIO_NUM_34
#define KEY_LISTEN KEY1_GPIO

class Inmp441Max98357aFastLED : public Device
{
public:
  Inmp441Max98357aFastLED() = default;

  void init();
  bool readAudio(uint8_t *data, size_t size);

  void setWriteMode(int sampleRate, int bitDepth, int numChannels);
  void writeAudio(uint8_t *data, size_t size, size_t *bytes_written);

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
  long currentMillis, startMillis;
};

void Inmp441Max98357aFastLED::init()
{
  esp_err_t err = ESP_OK;
  Serial.printf("Connect to Inmp441... \n");

  // Speakers
  i2s_config_t i2sConfig_tx = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = I2S_OUTPUT_SAMPLE_RATE,        // I2S_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 2,
      .dma_buf_len = 512,
      .tx_desc_auto_clear = true,
  };

  i2s_pin_config_t pin_config_tx = {
      .bck_io_num = I2S_BCLK, .ws_io_num = I2S_LRC, .data_out_num = I2S_DIN, .data_in_num = -1};

  err += i2s_driver_install(I2S_PORT_TX, &i2sConfig_tx, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing headphone driver: %d\n", err);
    while (true)
      ;
  }

  err += i2s_set_pin(I2S_PORT_TX, &pin_config_tx);
  if (err != ESP_OK) {
    Serial.printf("Failed setting headphone pin: %d\n", err);
    while (true)
      ;
  }
  Serial.println("I2S headphone driver installed.\n");

  i2s_config_t i2s_config = {.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
                             .sample_rate = I2S_SAMPLE_RATE,
                             .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
                             .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
                             .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
                             .intr_alloc_flags = 0,
                             .dma_buf_count = 2,
                             .dma_buf_len = 512,
                             .use_apll = 1};

  i2s_pin_config_t pin_config = {.bck_io_num = I2S_SCK, .ws_io_num = I2S_WS, .data_out_num = -1, .data_in_num = I2S_SD};

  err += i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing mic driver: %d\n", err);
    while (true)
      ;
  }

  err += i2s_set_pin(I2S_PORT, &pin_config);
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
    i2s_set_clk(I2S_PORT_TX, sampleRate, static_cast<i2s_bits_per_sample_t>(bitDepth),
                static_cast<i2s_channel_t>(numChannels));
  }
}

void Inmp441Max98357aFastLED::writeAudio(uint8_t *data, size_t size, size_t *bytes_written)
{
  i2s_write(I2S_PORT_TX, data, size, bytes_written, portMAX_DELAY);
}

bool Inmp441Max98357aFastLED::readAudio(uint8_t *data, size_t size)
{
  size_t bytes_read;
  i2s_read(I2S_PORT, (void *)i2s_read_buff, size, &bytes_read, portMAX_DELAY);
  uint32_t j = 0;
  uint32_t dac_value = 0;
  for (int i = 0; i < size; i += 2) {
    dac_value = ((((uint16_t)(i2s_read_buff[i + 1] & 0xff) << 8) | ((i2s_read_buff[i + 0]))));
    data[j++] = 0;
    data[j++] = dac_value * 256 / 2048;
  }
  return true;
}

void Inmp441Max98357aFastLED::updateBrightness(int brightness)
{
  FastLED.setBrightness(brightness);
}