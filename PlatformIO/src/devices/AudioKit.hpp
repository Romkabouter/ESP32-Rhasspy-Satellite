#pragma once
#include <Arduino.h>
#include <device.h>

#include <driver/i2s.h>
#include <AC101.h>
#include <Wire.h>
#include <IndicatorLight.h>
#include "ES8388Control.h"

// I2S pins
#define CONFIG_I2S_BCK_PIN 27
#define CONFIG_I2S_LRCK_PIN 26
#define CONFIG_I2S_DATA_PIN 25
#define CONFIG_I2S_DATA_IN_PIN 35

#define ES_CONFIG_I2S_BCK_PIN 5
#define ES_CONFIG_I2S_LRCK_PIN 25
#define ES_CONFIG_I2S_DATA_PIN 26
#define ES_CONFIG_I2S_DATA_IN_PIN 35

// AC101 I2C pins
#define IIC_CLK 32
#define IIC_DATA 33

#define ES_IIC_CLK 23
#define ES_IIC_DATA 18

// amplifier enable pin
#define GPIO_PA_EN GPIO_NUM_21
#define GPIO_SEL_PA_EN GPIO_SEL_21

// Audiokit LEDs
#define LED_D4 GPIO_NUM_19
#define LED_D5 GPIO_NUM_22
// alias
#define LED_WIFI LED_D5
#define LED_STREAM LED_D4

// Audiokit Buttons
#define KEY1_GPIO GPIO_NUM_36
#define KEY2_GPIO GPIO_NUM_13 // may be in use for other purposes, see onboard config switch
#define KEY3_GPIO GPIO_NUM_19 // also activates LED D4 if pressed
#define KEY4_GPIO GPIO_NUM_23 // do not use on A1S V2.3 -> I2C ES8388
#define KEY5_GPIO GPIO_NUM_18 // do not use on A1S V2.3 -> I2C ES8388
#define KEY6_GPIO GPIO_NUM_5  // do not use on A1S V2.3 -> I2S

// alias
#define KEY_LISTEN KEY3_GPIO
#define ES_KEY_LISTEN KEY1_GPIO

//#define KEY_VOL_UP KEYx_GPIO
//#define KEY_VOL_DOWN KEYx_GPIO

#define SPEAKER_I2S_NUMBER I2S_NUM_0


class AudioKit : public Device
{
public:
    AudioKit();
    void init();
    void updateColors(int colors);

    void setReadMode();
    void setWriteMode(int sampleRate, int bitDepth, int numChannels);

    void writeAudio(uint8_t *data, size_t size, size_t *bytes_written);
    bool readAudio(uint8_t *data, size_t size);

    void muteOutput(bool mute);

    // ESP-Audio-Kit has speaker and headphone as outputs
    // TODO
    void ampOutput(int output);
    void setVolume(uint16_t volume);

    bool isHotwordDetected();

    int numAmpOutConfigurations() { return 3; };

private:
    void InitI2SSpeakerOrMic(int mode);
    AC101 ac;
    ES8388Control es8388;

    uint8_t out_vol;
    AmpOut out_amp = AMP_OUT_SPEAKERS;

    bool is_es = false;
    bool is_mono_stream_stereo_out = false;
    uint16_t key_listen;

    IndicatorLight *indicator_light = new IndicatorLight(LED_STREAM);
};

AudioKit::AudioKit(){};

void AudioKit::init()
{
    Serial.print("Connect to Codec... ");

    if ((is_es = es8388.begin(ES_IIC_DATA, ES_IIC_CLK)) == true)
    {
        Serial.println("found ES8388Control");
    }
    else
    {
        Serial.println("looking for AC101...");
        while (!ac.begin(IIC_DATA, IIC_CLK))
        {
            Serial.printf("failed!\n");
            delay(1000);
        }
        ac.SetMode(AC101::MODE_ADC_DAC);
    }

    key_listen = is_es? ES_KEY_LISTEN: KEY_LISTEN;

    // LEDs
    pinMode(LED_STREAM, OUTPUT); // active low
    pinMode(LED_WIFI, OUTPUT);   // active low
    
    digitalWrite(LED_STREAM, HIGH);
    digitalWrite(LED_WIFI, HIGH);

    // Enable amplifier
    pinMode(GPIO_PA_EN, OUTPUT);

    // Configure keys on ESP32 Audio Kit board
    pinMode(key_listen, INPUT_PULLUP);
    // pinMode(KEY_VOL_UP, INPUT_PULLUP);
    // pinMode(KEY_VOL_DOWN, INPUT_PULLUP);

    // now initialize read mode 
    // setReadMode();
};

/**
 * @brief called to indicate state of device / service. LED D4 indicates "HOTWORD DETECTED/LISTEN MODE", LED D5 indicates  "WIFI DISCONNECT". Different brightness or colors
 * are not supported on the AudioKit
 * 
 * @param colors used to indicate state to display, see device.h (enum LedColorState) for values 
 */
void AudioKit::updateColors(int colors)
{
    // turn off LEDs
    /// digitalWrite(LED_STREAM, HIGH);
    indicator_light->setState(ON);

    digitalWrite(LED_WIFI, HIGH);

    switch (colors)
    {
    case COLORS_HOTWORD:
        /// digitalWrite(LED_STREAM, LOW);
        indicator_light->setState(PULSING);
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
    Serial.printf("InitI2SSpeakerOrMic -> %s\n", mode == MODE_MIC ? "Mic" : "Speaker");
    esp_err_t err = ESP_OK;

    i2s_driver_uninstall(SPEAKER_I2S_NUMBER);
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 16bit, stereo, MSB
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = (((mode == MODE_MIC) ? this->readSize : this->writeSize) * (is_es ? 2 : 1)) / 4,
    };
    if (mode == MODE_MIC)
    {
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    }
    else
    {
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        i2s_config.tx_desc_auto_clear = true;
    }

    err += i2s_driver_install(SPEAKER_I2S_NUMBER, &i2s_config, 0, NULL);

    i2s_pin_config_t tx_pin_config;

    if (is_es)
    {
        tx_pin_config.bck_io_num = ES_CONFIG_I2S_BCK_PIN;
        tx_pin_config.ws_io_num = ES_CONFIG_I2S_LRCK_PIN;
        tx_pin_config.data_out_num = ES_CONFIG_I2S_DATA_PIN;
        tx_pin_config.data_in_num = ES_CONFIG_I2S_DATA_IN_PIN;
    }
    else
    {
        tx_pin_config.bck_io_num = CONFIG_I2S_BCK_PIN;
        tx_pin_config.ws_io_num = CONFIG_I2S_LRCK_PIN;
        tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
        tx_pin_config.data_in_num = CONFIG_I2S_DATA_IN_PIN;
    }

    err += i2s_set_pin(SPEAKER_I2S_NUMBER, &tx_pin_config);

    if (is_es)
    {        
        // ES8388Control requires MCLK output.
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
        WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
    }
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
        if (is_es)
        {
            // es8388 requires stereo i2s
            i2s_set_clk(SPEAKER_I2S_NUMBER, sampleRate, static_cast<i2s_bits_per_sample_t>(bitDepth), I2S_CHANNEL_STEREO);
        }
        else
        {
            i2s_set_clk(SPEAKER_I2S_NUMBER, sampleRate, static_cast<i2s_bits_per_sample_t>(bitDepth), static_cast<i2s_channel_t>(numChannels));
        }
        // ES8388Control is never put into mono mode, hence we have to handle this case
        is_mono_stream_stereo_out = is_es && numChannels == 1;
    }
}

void AudioKit::setReadMode()
{
    if (mode != MODE_MIC)
    {
        InitI2SSpeakerOrMic(MODE_MIC);
        
        i2s_set_clk(SPEAKER_I2S_NUMBER, 16000, I2S_BITS_PER_SAMPLE_16BIT, is_es ? I2S_CHANNEL_STEREO : I2S_CHANNEL_MONO);

        mode = MODE_MIC;
    }
}

void AudioKit::writeAudio(uint8_t *data, size_t size, size_t *bytes_written)
{
    if (is_mono_stream_stereo_out == false)
    {
        // input stream and I2S confguration have same number of channels
        i2s_write(SPEAKER_I2S_NUMBER, data, size, bytes_written, portMAX_DELAY);      
    }
    else
    {
        // input stream is mono, I2S needs stereo, hence samples are send
        // twice to I2S stream to create 2 channels
        // HACK: This works ATM only for 16bit samples as sample size is hardcoded
        // here
        uint16_t data2[size];
        uint16_t *data1 = (uint16_t *)data;

        for (int idx = 0; idx < size / 2; idx++)
        {
            data2[2 * idx] = data1[idx];
            data2[2 * idx + 1] = data1[idx];
            
        }

        i2s_write(SPEAKER_I2S_NUMBER, data2, 2 * size, bytes_written, portMAX_DELAY);
        *bytes_written /= 2; // half the actual bytes written as we have double the stream size
    }
}

bool AudioKit::readAudio(uint8_t *data, size_t size)
{
    size_t byte_read;

    if (!is_es)
    {
        i2s_read(SPEAKER_I2S_NUMBER, data, size, &byte_read, pdMS_TO_TICKS(100));
    }
    else
    {
        // ES8388Control returns stereo stream from Mic, but we need only one channel, 
        // we drop channel 2 (right channel) here
        uint16_t data2[size];
        uint16_t *data1 = (uint16_t *)data;

        i2s_read(SPEAKER_I2S_NUMBER, data2, 2 * size, &byte_read, pdMS_TO_TICKS(100));

        for (size_t idx = 0; idx < size / 2; idx++)
        {
            data1[idx] = data2[idx * 2];
        }
        byte_read /= 2;
    }

    return byte_read == size;
}

void AudioKit::muteOutput(bool mute)
{
    // switching the amp power prevents klicking noise reaching the speaker 
    if (out_amp != AmpOut::AMP_OUT_HEADPHONE)
    {
        digitalWrite(GPIO_PA_EN, mute ? LOW : HIGH);
    }

    if (out_amp != AmpOut::AMP_OUT_SPEAKERS)
    {
        if (is_es)
        {
            // ES8388: we mute main dac
            es8388.mute(ES8388Control::ES_MAIN, mute);
        }
        else
        {
            // AC101: we just mute the headphone 
            ac.SetVolumeHeadphone(mute? 0: (out_vol * 63)/100);
        }
    }
}

/**
 * @brief sets output volume for all outputs
 * 
 * @param volume 0 - 100
 */
void AudioKit::setVolume(uint16_t volume)
{
    out_vol = volume;

    if (is_es)
    {
        es8388.volume(ES8388Control::ES_MAIN, out_vol);  
    }
    else
    {
        // volume is 0 to 100, needs to be 0 to 63
        const uint8_t vol = (uint8_t)(out_vol / 100.0f * 63.0f);

        switch(out_amp)
        {
        case AmpOut::AMP_OUT_SPEAKERS:
            ac.SetVolumeSpeaker(vol); 
            break;
        case AmpOut::AMP_OUT_HEADPHONE:
            ac.SetVolumeHeadphone(vol);
            break;
        case AmpOut::AMP_OUT_BOTH:
            ac.SetVolumeSpeaker(vol);
            ac.SetVolumeHeadphone(vol);
            break;

        }
    }
}

/**
 * @brief Regularily called to check if hotword is detected. Here no real detection takes 
 * places, it is simply the press of a button which activates command listening mode.
 * 
 * @return true if listening key is pressed  
 * @return false if listening key is not pressed
 */
bool AudioKit::isHotwordDetected()
{
    // TODOD debounce maybe?
    return digitalRead(key_listen) == LOW;
}

void AudioKit::ampOutput(int ampOut)
{
    out_amp = (AmpOut)ampOut;

    // spk, headphone
    bool mute[2] = {false, false};

    switch (out_amp)
    {
    case AmpOut::AMP_OUT_SPEAKERS:
        mute[0] = false;
        mute[1] = true;
        break;

    case AmpOut::AMP_OUT_HEADPHONE:
        mute[0] = true;
        mute[1] = false;
        break;
    default:
        mute[0] = false;
        mute[1] = false;
    }

    digitalWrite(GPIO_PA_EN, mute[0] ? LOW : HIGH);

    if (is_es)
    {
        es8388.mute(ES8388Control::ES_OUT2, mute[1]);
        es8388.volume(ES8388Control::ES_OUT2, mute[1] ? 0 : 100);

        es8388.mute(ES8388Control::ES_OUT1, mute[0]);
        es8388.volume(ES8388Control::ES_OUT1, mute[0] ? 0 : 100);
    }
    else
    {
        const uint8_t vol = (out_vol * 63)/100;
        ac.SetVolumeSpeaker(mute[0]?vol:0);
        ac.SetVolumeSpeaker(mute[1]?vol:0);
    }
}
