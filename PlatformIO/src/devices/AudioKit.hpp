#include <driver/i2s.h>
#include <AC101.h>

// I2S pins
#define IIS_SCLK 27
#define IIS_WSLC 26
#define IIS_DSIN 25
#define IIS_DSOU 35

#define CONFIG_I2S_BCK_PIN IIS_SCLK
#define CONFIG_I2S_LRCK_PIN IIS_WSLC
#define CONFIG_I2S_DATA_PIN IIS_DSIN
#define CONFIG_I2S_DATA_IN_PIN IIS_DSOU

// AC101 I2C pins
#define IIC_CLK 32
#define IIC_DATA 33

// amplifier enable pin
#define GPIO_PA_EN GPIO_NUM_21
#define GPIO_SEL_PA_EN GPIO_SEL_21

// LEDs
#define LED_D4 GPIO_NUM_19
#define LED_D5 GPIO_NUM_22
// alias
#define LED_WIFI LED_D5
#define LED_STREAM LED_D4

// Buttons
#define KEY1_GPIO GPIO_NUM_36
#define KEY2_GPIO GPIO_NUM_13
#define KEY3_GPIO GPIO_NUM_19
#define KEY4_GPIO GPIO_NUM_23
#define KEY5_GPIO GPIO_NUM_18
#define KEY6_GPIO GPIO_NUM_5
// alias
#define KEY_LISTEN KEY4_GPIO
// #define KEY_VOL_UP KEY5_GPIO
// #define KEY_VOL_DOWN KEY6_GPIO

#define SPEAKER_I2S_NUMBER I2S_NUM_0

class AudioKit : public Device
{
public:
    AudioKit();
    void init();
    void updateLeds(int colors);

    void setReadMode();
    void setWriteMode(int sampleRate, int bitDepth, int numChannels);

    void writeAudio(uint8_t *data, size_t size, size_t *bytes_written);
    bool readAudio(uint8_t *data, size_t size);

    void muteOutput(bool mute);

    // ESP-Audio-Kit has speaker and headphone as outputs
    // TODO
    // void ampOutput(int output) {};
    void setVolume(uint16_t volume);

    bool isHotwordDetected();

    int readSize = 512;
    int writeSize = 1024;

private:
    void InitI2SSpeakerOrMic(int mode);
    AC101 ac;
};

AudioKit::AudioKit(){};

void AudioKit::init()
{
    Serial.printf("Connect to AC101 codec... ");
    while (!ac.begin(IIC_DATA, IIC_CLK))
    {
        Serial.printf("Failed!\n");
        delay(1000);
    }
    ac.SetMode(AC101::MODE_ADC_DAC);

    // LEDs
    pinMode(LED_STREAM, OUTPUT); // active low
    pinMode(LED_WIFI, OUTPUT);   // active low
    digitalWrite(LED_STREAM, HIGH);
    digitalWrite(LED_WIFI, HIGH);

    // Enable amplifier
    pinMode(GPIO_PA_EN, OUTPUT);

    // Configure keys on ESP32 Audio Kit board
    pinMode(KEY_LISTEN, INPUT_PULLUP);
    // pinMode(KEY_VOL_UP, INPUT_PULLUP);
    // pinMode(KEY_VOL_DOWN, INPUT_PULLUP);
};

void AudioKit::updateLeds(int colors)
{
    // turn off LEDs
    digitalWrite(LED_STREAM, HIGH);
    digitalWrite(LED_WIFI, HIGH);

    switch (colors)
    {
    case COLORS_HOTWORD:
        digitalWrite(LED_STREAM, LOW);
        break;
    case COLORS_WIFI_CONNECTED:
        // LED_WIFI is turned off already
        break;
    case COLORS_WIFI_DISCONNECTED:
        digitalWrite(LED_WIFI, LOW);
        break;
    case COLORS_IDLE:
        // all LEDs are turned off already
        break;
    case COLORS_OTA:
        break;
    }
};

void AudioKit::InitI2SSpeakerOrMic(int mode)
{
    esp_err_t err = ESP_OK;

    i2s_driver_uninstall(SPEAKER_I2S_NUMBER);
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 60,
    };
    if (mode == MODE_MIC)
    {
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    }
    else
    {
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        // i2s_config.use_apll = false;
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

void AudioKit::setWriteMode(int sampleRate, int bitDepth, int numChannels)
{
    if (mode != MODE_SPK)
    {
        InitI2SSpeakerOrMic(MODE_SPK);
        mode = MODE_SPK;
    }
    if (sampleRate > 0)
    {
        i2s_set_clk(SPEAKER_I2S_NUMBER, sampleRate, static_cast<i2s_bits_per_sample_t>(bitDepth), static_cast<i2s_channel_t>(numChannels));
    }
}

void AudioKit::setReadMode()
{
    if (mode != MODE_MIC)
    {
        InitI2SSpeakerOrMic(MODE_MIC);
        mode = MODE_MIC;
    }
}

void AudioKit::writeAudio(uint8_t *data, size_t size, size_t *bytes_written)
{
    i2s_write(SPEAKER_I2S_NUMBER, data, size, bytes_written, portMAX_DELAY);
}

bool AudioKit::readAudio(uint8_t *data, size_t size)
{
    size_t byte_read;
    i2s_read(SPEAKER_I2S_NUMBER, data, size, &byte_read, (100 / portTICK_RATE_MS));
    return true;
}

void AudioKit::muteOutput(bool mute)
{
    digitalWrite(GPIO_PA_EN, mute ? LOW : HIGH);
}

void AudioKit::setVolume(uint16_t volume)
{
    // volume is 0 to 100, needs to be 0 to 63
    const uint8_t vol = (uint8_t)(volume / 100.0f * 63.0f);

    ac.SetVolumeHeadphone(vol);
    ac.SetVolumeSpeaker(vol);
}

bool AudioKit::isHotwordDetected()
{
    // TODOD debounce maybe?
    return digitalRead(KEY_LISTEN) == LOW;
}