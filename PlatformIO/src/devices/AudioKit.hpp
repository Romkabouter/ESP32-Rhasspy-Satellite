#pragma once
#include <Arduino.h>
#include <device.h>

#include <driver/i2s.h>
#include <AC101.h>
#include <Wire.h>
#include <IndicatorLight.h>

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
#define KEY2_GPIO GPIO_NUM_13
#define KEY3_GPIO GPIO_NUM_19 // also LED D4
#define KEY4_GPIO GPIO_NUM_23 // do not use on V2.3 -> I2C ES8388
#define KEY5_GPIO GPIO_NUM_18 // do not use on V2.3 -> I2C ES8388
#define KEY6_GPIO GPIO_NUM_5  // do not use on V2.3 -> I2S

// alias
#define KEY_LISTEN KEY3_GPIO
#define ES_KEY_LISTEN KEY1_GPIO

//#define KEY_VOL_UP KEYx_GPIO
//#define KEY_VOL_DOWN KEYx_GPIO

#define SPEAKER_I2S_NUMBER I2S_NUM_0

#ifndef _ES8388_REGISTERS_H
#define _ES8388_REGISTERS_H

#define ES8388_ADDR 0x10

/* ES8388 register */
#define ES8388_CONTROL1 0x00
#define ES8388_CONTROL2 0x01
#define ES8388_CHIPPOWER 0x02
#define ES8388_ADCPOWER 0x03
#define ES8388_DACPOWER 0x04
#define ES8388_CHIPLOPOW1 0x05
#define ES8388_CHIPLOPOW2 0x06
#define ES8388_ANAVOLMANAG 0x07
#define ES8388_MASTERMODE 0x08

/* ADC */
#define ES8388_ADCCONTROL1 0x09
#define ES8388_ADCCONTROL2 0x0a
#define ES8388_ADCCONTROL3 0x0b
#define ES8388_ADCCONTROL4 0x0c
#define ES8388_ADCCONTROL5 0x0d
#define ES8388_ADCCONTROL6 0x0e
#define ES8388_ADCCONTROL7 0x0f
#define ES8388_ADCCONTROL8 0x10
#define ES8388_ADCCONTROL9 0x11
#define ES8388_ADCCONTROL10 0x12
#define ES8388_ADCCONTROL11 0x13
#define ES8388_ADCCONTROL12 0x14
#define ES8388_ADCCONTROL13 0x15
#define ES8388_ADCCONTROL14 0x16

/* DAC */
#define ES8388_DACCONTROL1 0x17
#define ES8388_DACCONTROL2 0x18
#define ES8388_DACCONTROL3 0x19
#define ES8388_DACCONTROL4 0x1a
#define ES8388_DACCONTROL5 0x1b
#define ES8388_DACCONTROL6 0x1c
#define ES8388_DACCONTROL7 0x1d
#define ES8388_DACCONTROL8 0x1e
#define ES8388_DACCONTROL9 0x1f
#define ES8388_DACCONTROL10 0x20
#define ES8388_DACCONTROL11 0x21
#define ES8388_DACCONTROL12 0x22
#define ES8388_DACCONTROL13 0x23
#define ES8388_DACCONTROL14 0x24
#define ES8388_DACCONTROL15 0x25
#define ES8388_DACCONTROL16 0x26
#define ES8388_DACCONTROL17 0x27
#define ES8388_DACCONTROL18 0x28
#define ES8388_DACCONTROL19 0x29
#define ES8388_DACCONTROL20 0x2a
#define ES8388_DACCONTROL21 0x2b
#define ES8388_DACCONTROL22 0x2c
#define ES8388_DACCONTROL23 0x2d
#define ES8388_DACCONTROL24 0x2e
#define ES8388_DACCONTROL25 0x2f
#define ES8388_DACCONTROL26 0x30
#define ES8388_DACCONTROL27 0x31
#define ES8388_DACCONTROL28 0x32
#define ES8388_DACCONTROL29 0x33
#define ES8388_DACCONTROL30 0x34

#endif

class ES8388
{

    bool write_reg(uint8_t slave_add, uint8_t reg_add, uint8_t data);
    bool read_reg(uint8_t slave_add, uint8_t reg_add, uint8_t& data);
    bool identify(int sda, int scl, uint32_t frequency);

    public:

    bool begin(int sda = -1, int scl = -1, uint32_t frequency = 400000U);

    enum ES8388_OUT {
    ES_MAIN,    // this is the DAC output volume (both outputs)    
    ES_OUT1,    // this is the additional gain for OUT1
    ES_OUT2     // this is the additional gain for OUT2 
    };

    void mute(const  ES8388_OUT out, const bool muted);
    void volume(const  ES8388_OUT out, const uint8_t vol);

};

bool ES8388::write_reg(uint8_t slave_add, uint8_t reg_add, uint8_t data)
{
    Wire.beginTransmission(slave_add);
    Wire.write(reg_add);
    Wire.write(data);
    return Wire.endTransmission() == 0;
}

bool ES8388::read_reg(uint8_t slave_add, uint8_t reg_add, uint8_t& data)
{
    bool retval = false;
    Wire.beginTransmission(slave_add);
    Wire.write(reg_add);
    Wire.endTransmission(false);
    Wire.requestFrom((uint16_t)slave_add, (uint8_t)1, true);
    if (Wire.available() >= 1)
    {
        data = Wire.read();
        retval = true;
    }
    return retval;
}

bool ES8388::begin(int sda, int scl, uint32_t frequency)
{
    bool res = identify(sda, scl, frequency);

    if (res == true)
    {

        /* mute DAC during setup, power up all systems, slave mode */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL3, 0x04);
        res &= write_reg(ES8388_ADDR, ES8388_CONTROL2, 0x50);
        res &= write_reg(ES8388_ADDR, ES8388_CHIPPOWER, 0x00);
        res &= write_reg(ES8388_ADDR, ES8388_MASTERMODE, 0x00);

        /* power up DAC and enable LOUT1+2 / ROUT1+2, ADC sample rate = DAC sample rate */
        res &= write_reg(ES8388_ADDR, ES8388_DACPOWER, 0x3e);
        res &= write_reg(ES8388_ADDR, ES8388_CONTROL1, 0x12);

        /* DAC I2S setup: 16 bit word length, I2S format; MCLK / Fs = 256*/
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL1, 0x18);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL2, 0x02);

        /* DAC to output route mixer configuration: ADC MIX TO OUTPUT */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL16, 0x1B);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL17, 0x90);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL20, 0x90);

        /* DAC and ADC use same LRCK, enable MCLK input; output resistance setup */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL21, 0x80);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL23, 0x00);

        /* DAC volume control: 0dB (maximum, unattenuated)  */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL5, 0x00);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL4, 0x00);

        /* power down ADC while configuring; volume: +9dB for both channels */
        res &= write_reg(ES8388_ADDR, ES8388_ADCPOWER, 0xff);
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL1, 0x88); // +24db

        /* select LINPUT2 / RINPUT2 as ADC input; stereo; 16 bit word length, format right-justified, MCLK / Fs = 256 */
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL2, 0xf0); // 50
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL3, 0x80); // 00
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL4, 0x0e);
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL5, 0x02);

        /* set ADC volume */
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL8, 0x20);
        res &= write_reg(ES8388_ADDR, ES8388_ADCCONTROL9, 0x20);

        /* set LOUT1 / ROUT1 volume: 0dB (unattenuated) */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL24, 0x1e);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL25, 0x1e);

        /* set LOUT2 / ROUT2 volume: 0dB (unattenuated) */
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL26, 0x1e);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL27, 0x1e);

        /* power up and enable DAC; power up ADC (no MIC bias) */
        res &= write_reg(ES8388_ADDR, ES8388_DACPOWER, 0x3c);
        res &= write_reg(ES8388_ADDR, ES8388_DACCONTROL3, 0x00);
        res &= write_reg(ES8388_ADDR, ES8388_ADCPOWER, 0x00);
    }

    return res;
}

/**
 * @brief (un)mute one of the two outputs or main dac output of the ES8388 by switching of the output register bits. Does not really mute the selected output, causes an attenuation. 
 * hence should be used in conjunction with appropriate volume setting. Main dac output mute does mute both outputs
 * 
 * @param out 
 * @param muted 
 */
void ES8388::mute(const ES8388_OUT out, const bool muted)
{
    uint8_t reg_addr;
    uint8_t mask_mute;
    uint8_t mask_val;

    switch(out)
    {
    case ES_OUT1:
        reg_addr = ES8388_DACPOWER;
        mask_mute =(3 << 4);
        mask_val = muted ? 0 : mask_mute;
        break;
    case ES_OUT2:
        reg_addr = ES8388_DACPOWER;
        mask_mute =(3 << 2);
        mask_val = muted ? 0 : mask_mute;
        break;
    case ES_MAIN: 
    default:
        reg_addr = ES8388_DACCONTROL3;
        mask_mute = 1 << 2;
        mask_val = muted ? mask_mute : 0;
        break;
    }

    uint8_t reg;
    if (read_reg(ES8388_ADDR, reg_addr, reg))
    {
        reg = (reg & ~mask_mute) | (mask_val & mask_mute);
        write_reg(ES8388_ADDR, reg_addr, reg);
    }
}

/**
 * @brief Set volume gain for the main dac, or for one of the two output channels. Final gain = main gain + out channel gain 
 * 
 * @param out which gain setting to control
 * @param vol 0-100 (100 is max)
 */
void ES8388::volume(const ES8388_OUT out, const uint8_t vol)
{
    const uint32_t max_vol = 100; // max input volume value 
    
    const int32_t max_vol_val = out == ES8388_OUT::ES_MAIN? 96 : 0x21; // max register value for ES8388 out volume


    uint8_t lreg = 0, rreg = 0;

    switch (out)
    {
        case ES_MAIN: lreg = ES8388_DACCONTROL4; rreg = ES8388_DACCONTROL5; break;
        case ES_OUT1: lreg = ES8388_DACCONTROL24; rreg = ES8388_DACCONTROL25; break;
        case ES_OUT2: lreg = ES8388_DACCONTROL26; rreg = ES8388_DACCONTROL27; break;
    }

    uint8_t vol_val = vol > max_vol? max_vol_val : (max_vol_val * vol) / max_vol;

    // main dac volume control is reverse scale (lowest value is loudest)
    // hence we reverse the calculated value
    if (out == ES_MAIN) { vol_val = max_vol_val - vol_val; }

    write_reg(ES8388_ADDR, lreg, vol_val);
    write_reg(ES8388_ADDR, rreg, vol_val);
}

/**
 * @brief Test if device with I2C address for ES8388 is connected to the I2C bus 
 * 
 * @param sda which pin to use for I2C SDA
 * @param scl which pin to use for I2C SCL
 * @param frequency which frequency to use as I2C bus frequency
 * @return true device was found
 * @return false device was not found
 */
bool ES8388::identify(int sda, int scl, uint32_t frequency)
{
    Wire.begin(sda, scl, frequency);
    Wire.beginTransmission(ES8388_ADDR);
    return Wire.endTransmission() == 0;
}

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

private:
    void InitI2SSpeakerOrMic(int mode);
    AC101 ac;
    ES8388 es8388;

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
        Serial.println("found ES8388");
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
        // ES8388 requires MCLK output.
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
        // ES8388 is never put into mono mode, hence we have to handle this case
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
        // ES8388 returns stereo stream from Mic, but we need only one channel, 
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
    if (is_es)
    {
        es8388.mute(ES8388::ES_MAIN, mute);
    }
    else
    {
        digitalWrite(GPIO_PA_EN, mute ? LOW : HIGH);
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
        es8388.volume(ES8388::ES_MAIN, out_vol);  
    }
    else
    {
        // volume is 0 to 100, needs to be 0 to 63
        const uint8_t vol = (uint8_t)(out_vol / 100.0f * 63.0f);

        ac.SetVolumeHeadphone(vol);
        ac.SetVolumeSpeaker(vol);
    }
}

/**
 * @brief Regularily called to check if hotword is detected. Here no really detection takes 
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
    // spk, headphone
    bool mute[] = {false, false};

    switch (ampOut)
    {
    case AmpOut::AMP_OUT_SPEAKERS:
        digitalWrite(GPIO_PA_EN, HIGH);
        mute[0] = false;
        mute[1] = true;
        break;

    case AmpOut::AMP_OUT_HEADPHONE:
        digitalWrite(GPIO_PA_EN, LOW);
        mute[0] = true;
        mute[1] = false;
        break;
    }

    digitalWrite(GPIO_PA_EN, mute[0] ? LOW : HIGH);
    if (is_es)
    {
        es8388.mute(ES8388::ES_OUT2, mute[1]);
        es8388.volume(ES8388::ES_OUT2, mute[1] ? 0 : 100);

        es8388.mute(ES8388::ES_OUT1, mute[0]);
        es8388.volume(ES8388::ES_OUT1, mute[0] ? 0 : 100);
    }
    else
    {
        const uint8_t vol = (out_vol * 63)/100;
        ac.SetVolumeSpeaker(mute[0]?vol:0);
        ac.SetVolumeSpeaker(mute[1]?vol:0);
    }
}