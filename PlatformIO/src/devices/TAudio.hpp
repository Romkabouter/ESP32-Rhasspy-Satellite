#pragma once
#include <Arduino.h>
#include <device.h>

#include <driver/i2s.h>
#include <WM8978.h>

#include <NeoPixelBus.h>

// Pins for the TTGO T-Audio (T9)
#define CONFIG_I2S_BCK_PIN 33
#define CONFIG_I2S_LRCK_PIN 25
#define CONFIG_I2S_DATA_PIN 26
#define CONFIG_I2S_DATA_IN_PIN 27

//#define WM8978_I2S_CLK 0
#define WM8978_SDA 19
#define WM8978_SCL 18
#define WS2812B_DATA_PIN 22
#define WS2812B_NUM_LEDS 19

//#define KEY1_GPIO RST
#define KEY2_GPIO 39
#define KEY3_GPIO 36
#define KEY4_GPIO 34
// alias
#define KEY_LISTEN KEY2_GPIO

// #define SD MISO 2
// #define SD SCK 14
// #define SD CS 13
// #define SD MOSI 15

#define I2S_NUM I2S_NUM_0


NeoPixelBus<NeoRgbFeature, NeoEsp32I2s1800KbpsMethod> strip(WS2812B_NUM_LEDS, WS2812B_DATA_PIN);


class TAudio : public Device {
  public:
    TAudio();
    void init();
    void updateColors(int colors);
    void updateBrightness(int brightness);
    void setReadMode();
    void setWriteMode(int sampleRate, int bitDepth, int numChannels);
    void writeAudio(uint8_t *data, size_t size, size_t *bytes_written);
    bool readAudio(uint8_t *data, size_t size);
    void muteOutput(bool mute);
    void ampOutput(int output);
    void setVolume(uint16_t volume);
    void setGain(uint16_t gain);
    bool isHotwordDetected();
    int numAmpOutConfigurations() { return 3; };
    int readSize = 512;
    int writeSize = 512;
    int width = 2;
    int rate = 16000;

  private:
    void InitI2S();
    WM8978 dac;

    AmpOut out_amp = AMP_OUT_SPEAKERS;
    uint8_t out_vol;
    bool muted;
    uint8_t brightness;
};

TAudio::TAudio() {
}

void TAudio::init() {
  // LEDs
  strip.Begin();
  strip.ClearTo(RgbColor(0));
  brightness = 64;
  strip.Show();

  Serial.printf("Connect to WM8978 codec... ");
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
  REG_WRITE(PIN_CTRL, 0xFFFFFFF0);
  while (!dac.begin(WM8978_SDA, WM8978_SCL))
  {
    Serial.printf("Setting up DAC failed!\n");
    delay(1000);
  }
  dac.cfgInput(1, 0, 0);
  dac.setMICgain(40); // 25
  dac.setHPF(1);

  dac.setSPKvol(58);  // 63
  dac.setHPvol(48, 48);
  out_vol = 58; muted = false;

  pinMode(KEY_LISTEN, INPUT);

  InitI2S();
}

void TAudio::InitI2S() {
  esp_err_t err = ESP_OK;

  i2s_driver_uninstall(I2S_NUM);
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = true
  };
  i2s_config.tx_desc_auto_clear = true;

  err += i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  i2s_pin_config_t tx_pin_config;

  tx_pin_config.bck_io_num = CONFIG_I2S_BCK_PIN;
  tx_pin_config.ws_io_num = CONFIG_I2S_LRCK_PIN;
  tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
  tx_pin_config.data_in_num = CONFIG_I2S_DATA_IN_PIN;

  err += i2s_set_pin(I2S_NUM, &tx_pin_config);
  err += i2s_set_clk(I2S_NUM, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
  REG_WRITE(PIN_CTRL, 0xFFFFFFF0);

  return;
}

void TAudio::updateColors(int colors) {
  strip.ClearTo(RgbColor(RgbwColor(ColorMap[colors][0],ColorMap[colors][1],ColorMap[colors][2],ColorMap[colors][3])).Dim(brightness));
  strip.Show();
}

void TAudio::updateBrightness(int brightness) {
  this->brightness = (brightness*255)/100;
}

void TAudio::setWriteMode(int sampleRate, int bitDepth, int numChannels) {
  if (mode != MODE_SPK) {
    mode = MODE_SPK;
  }
  if (sampleRate > 0) {
    //i2s_zero_dma_buffer(I2S_NUM);
    i2s_set_clk(I2S_NUM, sampleRate, static_cast<i2s_bits_per_sample_t>(bitDepth), static_cast<i2s_channel_t>(numChannels));
  }
}

void TAudio::setReadMode() {
  if (mode != MODE_MIC) {
    //i2s_zero_dma_buffer(I2S_NUM);
    i2s_set_clk(I2S_NUM, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
    mode = MODE_MIC;
  }
}

void TAudio::writeAudio(uint8_t *data, size_t size, size_t *bytes_written) {
  i2s_write(I2S_NUM, data, size, bytes_written, portMAX_DELAY);
}

bool TAudio::readAudio(uint8_t *data, size_t size) {
  size_t byte_read;
  i2s_read(I2S_NUM, data, size, &byte_read, (100 / portTICK_RATE_MS));
  return true;
}

void TAudio::muteOutput(bool mute) {
  if (muted == mute) return;  // already set

  if (mute) i2s_zero_dma_buffer(I2S_NUM);
  dac.cfgOutput(mute ? 0 : 1, 0);
  //setVolume(mute ? 0 : out_vol);
  muted = mute;
}

void TAudio::ampOutput(int output) {
  out_amp = (AmpOut)output;
  const uint8_t vol = (out_vol * 63)/100;

  switch (out_amp)
  {
    case AmpOut::AMP_OUT_SPEAKERS:
      dac.setSPKvol(vol);
      dac.setHPvol(0, 0);
      break;
    case AmpOut::AMP_OUT_HEADPHONE:
      dac.setSPKvol(0);
      dac.setHPvol(vol, vol);
      break;
    case AmpOut::AMP_OUT_BOTH:
      dac.setSPKvol(vol);
      dac.setHPvol(vol, vol);
      break;
  }
}

void TAudio::setVolume(uint16_t volume) {
  out_vol = volume;
  // volume is 0 to 100, needs to be 0 to 63
  const uint8_t vol = (volume * 63) / 100;

  switch (out_amp)
  {
    case AmpOut::AMP_OUT_SPEAKERS:
      dac.setSPKvol(vol);
      break;
    case AmpOut::AMP_OUT_HEADPHONE:
      dac.setHPvol(vol, vol);
      break;
    case AmpOut::AMP_OUT_BOTH:
      dac.setSPKvol(vol);
      dac.setHPvol(vol, vol);
      break;
  }
}

void TAudio::setGain(uint16_t gain) {
  const uint8_t g = (gain * 63) / 8;
  dac.setMICgain(g);
}

bool TAudio::isHotwordDetected() {
  return (digitalRead(KEY_LISTEN) == LOW);
}
