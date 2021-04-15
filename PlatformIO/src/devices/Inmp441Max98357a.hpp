#include <driver/i2s.h>
#include "IndicatorLight.h"


// I2S pins on ESp32cam MIC
// GPIO2 <--> WS
// GPIO14 <--> SCK
// GPIO15 <--> SD
#define I2S_SCK 14
#define I2S_WS 2
#define I2S_SD 15
#define I2S_PORT I2S_NUM_0

// I2S pins on ESp32Cam Speakers
// GPIO16 <--> LRC
// GPIO13 <--> BCLK
// GPIO12 <--> DIN
#define I2S_LRC 16
#define I2S_BCLK 13
#define I2S_DIN 12
#define I2S_PORT_TX I2S_NUM_1

#define I2S_SAMPLE_RATE   (16000)
#define I2S_SAMPLE_BITS   (16)
#define I2S_READ_LEN     512

// LEDs
#define LED_FLASH 4
#define LED 33


class Inmp441Max98357a : public Device
{
  public:
    Inmp441Max98357a();
    void init();
    void updateColors(int colors);
    bool readAudio(uint8_t *data, size_t size);
    void setWriteMode(int sampleRate, int bitDepth, int numChannels);
    void writeAudio(uint8_t *data, size_t size, size_t *bytes_written);
    IndicatorLight *indicator_light = new IndicatorLight(LED_FLASH);
  private:
    char* i2s_read_buff = (char*) calloc(I2S_READ_LEN, sizeof(char));
};

Inmp441Max98357a::Inmp441Max98357a() {};

void Inmp441Max98357a::init() {

    esp_err_t err = ESP_OK;
  Serial.printf("Connect to Inmp441... \n");

  // Speakers
    i2s_config_t i2sConfig_tx = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S  | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 2,
        .dma_buf_len = 512,
       };

        i2s_pin_config_t pin_config_tx = {
          .bck_io_num = I2S_BCLK,
          .ws_io_num = I2S_LRC,
          .data_out_num = I2S_DIN,
          .data_in_num = -1
        };

        err += i2s_driver_install(I2S_PORT_TX, &i2sConfig_tx, 0, NULL);
        if (err != ESP_OK) {
           Serial.printf("Failed installing headphone driver: %d\n", err);
           while (true);
        }

        err += i2s_set_pin(I2S_PORT_TX, &pin_config_tx);
        if (err != ESP_OK) {
          Serial.printf("Failed setting headphone pin: %d\n", err);
          while (true);
        }
        Serial.println("I2S headphone driver installed.\n");


  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 2,
    .dma_buf_len = 512,
    .use_apll = 1
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  err += i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
     Serial.printf("Failed installing mic driver: %d\n", err);
     while (true);
  }

  err += i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting mic pin: %d\n", err);
    while (true);
  }
  Serial.println("I2S driver mic installed.");

  return;
}


void Inmp441Max98357a::updateColors(int colors)
{
    indicator_light->setState(OFF);
    switch (colors)
    {
    case COLORS_HOTWORD:
        indicator_light->setState(PULSING);
    case COLORS_WIFI_CONNECTED:
        break;
    case COLORS_WIFI_DISCONNECTED:
        break;
    case COLORS_IDLE:
        break;
    case COLORS_OTA:
        break;
    }
};


void Inmp441Max98357a::setWriteMode(int sampleRate, int bitDepth, int numChannels)
{
    if (sampleRate > 0)
    {
        i2s_set_clk(I2S_PORT_TX, sampleRate, static_cast<i2s_bits_per_sample_t>(bitDepth), static_cast<i2s_channel_t>(numChannels));
    }
}


void Inmp441Max98357a::writeAudio(uint8_t *data, size_t size, size_t *bytes_written) {
  i2s_write(I2S_PORT_TX, data, size, bytes_written, portMAX_DELAY);
}


bool Inmp441Max98357a::readAudio(uint8_t *data, size_t size) {
    size_t bytes_read;
    i2s_read(I2S_PORT, (void*) i2s_read_buff, size, &bytes_read, portMAX_DELAY);
    uint32_t j = 0;
    uint32_t dac_value = 0;
    for (int i = 0; i < size; i += 2) {
        dac_value = ((((uint16_t) (i2s_read_buff[i + 1] & 0xf) << 8) | ((i2s_read_buff[i + 0]))));
        data[j++] = 0;
        data[j++] = dac_value * 256 / 2048;
    }
    return true;
}
