#pragma once
#include <Arduino.h>
#include <device.h>

#include <driver/i2s.h>


// I2S pins on ESp32Cam
//GPIO2 <--> WS
//GPIO14 <--> SCK
//GPIO15 <--> SD

#define I2S_SCK 14
#define I2S_WS 2
#define I2S_SD 15

#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE   (16000)
#define I2S_SAMPLE_BITS   (16)
#define I2S_READ_LEN      512

// LEDs
#define LED_STREAM 4
#define LED_WIFI 33


// class DetectWakeWordState;

class Inmp441 : public Device
{
  public:
    Inmp441();
    void init();
    void updateColors(int colors);
    bool readAudio(uint8_t *data, size_t size);

  private:
    char* i2s_read_buff = (char*) calloc(I2S_READ_LEN, sizeof(char));
};



Inmp441::Inmp441() {};


void Inmp441::init() {
  pinMode(LED_STREAM, OUTPUT); // active high
  pinMode(LED_WIFI, OUTPUT);   // active low
  digitalWrite(LED_STREAM, LOW);
  digitalWrite(LED_WIFI, HIGH);

  esp_err_t err = ESP_OK;
  Serial.printf("Connect to Inmp441... \n");

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
     Serial.printf("Failed installing driver: %d\n", err);
     while (true);
  }

  err += i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting pin: %d\n", err);
    while (true);
  }
  Serial.println("I2S driver installed.\n");
  return;
}


void Inmp441::updateColors(int colors)
{
  //  printf("colors: %d\n",colors);

    switch (colors)
    {
    case COLORS_HOTWORD:
        digitalWrite(LED_STREAM, HIGH);
        break;
    case COLORS_WIFI_CONNECTED:
        // LED_WIFI is turned off already
        break;
    case COLORS_WIFI_DISCONNECTED:
        digitalWrite(LED_WIFI, LOW);
        break;
    case COLORS_IDLE:
        digitalWrite(LED_STREAM, LOW);
        break;
    case COLORS_OTA:
        break;
    }
};



bool Inmp441::readAudio(uint8_t *data, size_t size) {

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
