#include "M5Atom.h"
#include <driver/i2s.h>

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
  void updateColors(int colors);
  void updateBrightness(int brightness);
  void setReadMode();
  void setWriteMode(int sampleRate, int bitDepth, int numChannels);
  void writeAudio(uint8_t *data, size_t size, size_t *bytes_written);
  bool readAudio(uint8_t *data, size_t size);
  bool isHotwordDetected();
private:
  void InitI2SSpeakerOrMic(int mode);
};

M5AtomEcho::M5AtomEcho()
{
};

void M5AtomEcho::init()
{
  M5.begin(true,true,true);
};

bool M5AtomEcho::isHotwordDetected() {
  M5.update();
  return M5.Btn.isPressed();
}

void M5AtomEcho::updateColors(int colors)
{
  switch (colors) {
    case COLORS_HOTWORD:
      M5.dis.drawpix(0, CRGB(hotword_colors[1],hotword_colors[0],hotword_colors[2]));
    break;
    case COLORS_WIFI_CONNECTED:
      M5.dis.drawpix(0, CRGB(wifi_conn_colors[0],wifi_conn_colors[1],wifi_conn_colors[2]));
    break;
    case COLORS_IDLE:
      M5.dis.drawpix(0, CRGB(idle_colors[0],idle_colors[1],idle_colors[2]));
    break;
    case COLORS_WIFI_DISCONNECTED:
      M5.dis.drawpix(0, CRGB(wifi_disc_colors[1],wifi_disc_colors[0],wifi_disc_colors[2]));
    break;
    case COLORS_OTA:
      M5.dis.drawpix(0, CRGB(ota_colors[3],ota_colors[3],ota_colors[3]));
    break;
  }
};

void M5AtomEcho::updateBrightness(int brightness) {
  M5.dis.setBrightness(brightness);
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