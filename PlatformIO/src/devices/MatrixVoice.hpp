#pragma once
#include <Arduino.h>
#include <device.h>

#include "everloop.h"
#include "everloop_image.h"
#include "microphone_array.h"
#include "microphone_core.h"
#include "voice_memory_map.h"
#include "wishbone_bus.h"
#include <thread>

// This is used to be able to change brightness, while keeping the colors appear
// the same Called gamma correction, check this
// https://learn.adafruit.com/led-tricks-gamma-correction/the-issue
const uint8_t PROGMEM gamma8[] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,
    2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,
    4,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,
    8,   9,   9,   9,   10,  10,  10,  11,  11,  11,  12,  12,  13,  13,  13,
    14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  20,  21,
    21,  22,  22,  23,  24,  24,  25,  25,  26,  27,  27,  28,  29,  29,  30,
    31,  32,  32,  33,  34,  35,  35,  36,  37,  38,  39,  39,  40,  41,  42,
    43,  44,  45,  46,  47,  48,  49,  50,  50,  51,  52,  54,  55,  56,  57,
    58,  59,  60,  61,  62,  63,  64,  66,  67,  68,  69,  70,  72,  73,  74,
    75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89,  90,  92,  93,  95,
    96,  98,  99,  101, 102, 104, 105, 107, 109, 110, 112, 114, 115, 117, 119,
    120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142, 144, 146,
    148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175, 177,
    180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
    215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252,
    255};

class MatrixVoice : public Device
{
public:
  MatrixVoice();
  void init();
  void animate(StateColors colors, int mode);
  void animateRunning(StateColors colors);
  void animateBlinking(StateColors colors);
  void animatePulsing(StateColors colors);
  void updateColors(StateColors colors);
  void updateBrightness(int brightness);
  void muteOutput(bool mute);
  void setVolume(uint16_t volume);
  void setWriteMode(int sampleRate, int bitDepth, int numChannels); 
  bool readAudio(uint8_t *data, size_t size);
  void writeAudio(uint8_t *data, size_t size, size_t *bytes_written);
  void ampOutput(int output);
  bool animationSupported() { return true; };
  bool runningSupported() { return true; };
  bool pulsingSupported() { return true; };
  bool blinkingSupported() { return true; };
  int readSize = 512;
  int writeSize = 1024;
  int width = 2;
  int rate = 16000;

private:
  matrix_hal::WishboneBus wb;
  matrix_hal::Everloop everloop;
  matrix_hal::MicrophoneArray *mics;
  matrix_hal::EverloopImage image1d;
  void playBytes(int16_t* input, uint32_t length);
  void interleave(const int16_t * in_L, const int16_t * in_R, int16_t * out, const size_t num_samples);
  bool FIFOFlush();
  void updateColors(StateColors colors, bool usePulse);
  void SetPCMSamplingFrequency(uint16_t PCM_constant);
  uint16_t GetFIFOStatus();
  int fifoSize = 4096;
  int sampleRate, bitDepth, numChannels;
  int brightness, pulse = 15;
  float sample_time = 1.0 / 16000;
  uint32_t spiLength = 1024;
  int sleep = int(spiLength * sample_time * 1000);
  int position = 0;
  long currentMillis, startMillis;
  bool ledsOn = true;
  bool directionDown = false;

  std::map<int, int> FrequencyMap = {
      {8000, 975},
      {16000, 492 },
      {22050, 355 },
      {44100, 177 }
  };
};

MatrixVoice::MatrixVoice()
{
};

void MatrixVoice::init()
{
  Serial.println("Matrix Voice Initialized");
  wb.Init();
  everloop.Setup(&wb);
  mics = new matrix_hal::MicrophoneArray();
  mics->Setup(&wb);
  mics->SetSamplingRate(rate);  
  matrix_hal::MicrophoneCore mic_core(*mics);
  mic_core.Setup(&wb);  
  uint16_t PCM_constant = 492;
  wb.SpiWrite(matrix_hal::kConfBaseAddress + 9, (const uint8_t *)(&PCM_constant), sizeof(uint16_t));
  currentMillis = millis();
  startMillis = millis();
};

void MatrixVoice::updateBrightness(int brightness) {
  // all values below 10 is read as 0 in gamma8, we map 0 to 10
  if (brightness > 100) { brightness = 100; }
  MatrixVoice::brightness = brightness * 90 / 100 + 10;
  MatrixVoice::pulse = brightness * 90 / 100 + 10;
}

void MatrixVoice::animate(StateColors colors, int mode) {
  switch (mode)
  {
  case AnimationMode::RUN:
    animateRunning(colors);
    break;
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

void MatrixVoice::animateRunning(StateColors colors) {
  currentMillis = millis();
  if (currentMillis - startMillis > 10) {
    int r = ColorMap[colors][0];
    int g = ColorMap[colors][1];
    int b = ColorMap[colors][2];
    int w = ColorMap[colors][3];		
    startMillis = millis();
    for (int i = 0; i < image1d.leds.size(); i++) {
      int red = ((i + 1) * brightness / image1d.leds.size()) * r / 100;
      int green = ((i + 1) * brightness / image1d.leds.size()) * g / 100;
      int blue = ((i + 1) * brightness / image1d.leds.size()) * b / 100;
      int white = ((i + 1) * brightness / image1d.leds.size()) * w / 100;
      image1d.leds[(i + position) % image1d.leds.size()].red = pgm_read_byte(&gamma8[red]);
      image1d.leds[(i + position) % image1d.leds.size()].green = pgm_read_byte(&gamma8[green]);
      image1d.leds[(i + position) % image1d.leds.size()].blue = pgm_read_byte(&gamma8[blue]);
      image1d.leds[(i + position) % image1d.leds.size()].white = pgm_read_byte(&gamma8[white]);
    }
    position++;
    position %= image1d.leds.size();
    everloop.Write(&image1d);
  }
}

void MatrixVoice::animateBlinking(StateColors colors) {
  currentMillis = millis();
  if (currentMillis - startMillis > 300) {
    MatrixVoice::pulse = ledsOn ? MatrixVoice::brightness : 0;
    ledsOn = !ledsOn;
    startMillis = millis();
    updateColors(colors, true);
  }
}

void MatrixVoice::animatePulsing(StateColors colors) {
  currentMillis = millis();
  if (currentMillis - startMillis > 5) {
    if (MatrixVoice::pulse > MatrixVoice::brightness) { directionDown = true; }
    MatrixVoice::pulse = directionDown ? MatrixVoice::pulse - 5 : MatrixVoice::pulse + 5;
    if (MatrixVoice::pulse < 5) { directionDown = false; }
    startMillis = millis();
    updateColors(colors, true);
  }
}

void MatrixVoice::updateColors(StateColors colors, bool usePulse) {
  int r = ColorMap[colors][0];
  int g = ColorMap[colors][1];
  int b = ColorMap[colors][2];
  int w = ColorMap[colors][3];		
  int brightness = usePulse ? MatrixVoice::pulse : MatrixVoice::brightness;
  r = floor(brightness * r / 100);
  r = pgm_read_byte(&gamma8[r]);
  g = floor(brightness * g / 100);
  g = pgm_read_byte(&gamma8[g]);
  b = floor(brightness * b / 100);
  b = pgm_read_byte(&gamma8[b]);
  w = floor(brightness * w / 100);
  w = pgm_read_byte(&gamma8[w]);
  for (matrix_hal::LedValue &led : image1d.leds) {
    led.red = r;
    led.green = g;
    led.blue = b;
    led.white = w;
  }
  everloop.Write(&image1d);
}

void MatrixVoice::updateColors(StateColors colors) {
  updateColors(colors, false);
}

void MatrixVoice::muteOutput(bool mute) {
  int16_t muteValue = mute ? 1 : 0;
  wb.SpiWrite(matrix_hal::kConfBaseAddress+10,(const uint8_t *)(&muteValue), sizeof(uint16_t));
}

void MatrixVoice::setVolume(uint16_t volume) {
  uint16_t outputVolume = (100 - volume) * 25 / 100; //25 is minimum volume
  wb.SpiWrite(matrix_hal::kConfBaseAddress+8,(const uint8_t *)(&outputVolume), sizeof(uint16_t));
};

void MatrixVoice::ampOutput(int output) {
  wb.SpiWrite(matrix_hal::kConfBaseAddress+11,(const uint8_t *)(&output), sizeof(uint16_t));
};

void MatrixVoice::SetPCMSamplingFrequency(uint16_t PCM_constant) {
  wb.SpiWrite(matrix_hal::kConfBaseAddress+9, (const uint8_t *)(&PCM_constant), sizeof(uint16_t));
}

void MatrixVoice::setWriteMode(int sampleRate, int bitDepth, int numChannels) {
  MatrixVoice::sampleRate = sampleRate;
  MatrixVoice::bitDepth = bitDepth;
  MatrixVoice::numChannels = numChannels;
  FIFOFlush();
  if (sampleRate == 8000 || sampleRate == 16000 || sampleRate == 22050 || sampleRate == 44100 ) {
    if (sampleRate == 44100 && numChannels == 2) {
      //Strange issue with 44100 stereo. When using 177 that output is very bad
      //When using 220, output is ok but a tad to slow.
      SetPCMSamplingFrequency(220);
    } else {
      SetPCMSamplingFrequency(FrequencyMap[sampleRate]);
    }
  }

  sample_time = 1.0 / sampleRate;
  sleep = int(spiLength * sample_time * 1000);
  writeSize = spiLength;
  if (numChannels == 1) {
    writeSize = writeSize / sizeof(uint16_t);
  }
}; 

bool MatrixVoice::readAudio(uint8_t *data, size_t size) {
  mics->Read();
  uint16_t voicebuffer[readSize];
  for (uint32_t s = 0; s < readSize; s++) {
     voicebuffer[s] = mics->Beam(s);
  }
  memcpy(data, voicebuffer, readSize * width);
  return true;
}

void MatrixVoice::writeAudio(uint8_t *data, size_t inputLength, size_t *bytes_written) {
  *bytes_written = inputLength;
  uint32_t outputLength = (numChannels == 1) ? inputLength * sizeof(int16_t) : inputLength;
  int16_t output[outputLength];

  if (numChannels == 1) {
    uint32_t monoLength = inputLength / sizeof(int16_t);
    int16_t mono[monoLength]; 
    int16_t stereo[inputLength];
    for (int i = 0; i < inputLength; i += 2) {
      mono[i/2] = ((data[i] & 0xff) | (data[i + 1] << 8));
    }
    interleave(mono, mono, stereo, monoLength);
    for (int i = 0; i < inputLength; i++) {
      output[i] = stereo[i];
    }
  } else {
    for (int i = 0; i < inputLength; i += 2) {
      output[i/2] = ((data[i] & 0xff) | (data[i + 1] << 8));
    }
  }

  if (GetFIFOStatus() > fifoSize * 3 / 4) {
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
  }
  wb.SpiWrite(matrix_hal::kDACBaseAddress, (const uint8_t *)output, outputLength);
}

void MatrixVoice::interleave(const int16_t * in_L, const int16_t * in_R, int16_t * out, const size_t num_samples)
{
    for (size_t i = 0; i < num_samples; ++i)
    {
        out[i * 2] = in_L[i];
        out[i * 2 + 1] = in_R[i];
    }
}

bool MatrixVoice::FIFOFlush() {
  int16_t value = 1;
  wb.SpiWrite(matrix_hal::kConfBaseAddress + 12,(const uint8_t *)(&value), sizeof(uint16_t));	
  value = 0;
  wb.SpiWrite(matrix_hal::kConfBaseAddress + 12,(const uint8_t *)(&value), sizeof(uint16_t));	
  return true;
}

uint16_t MatrixVoice::GetFIFOStatus() {
  uint16_t write_pointer;
  uint16_t read_pointer;
  wb.SpiRead(matrix_hal::kDACBaseAddress + 2050, (uint8_t *)(&read_pointer), sizeof(uint16_t));
  wb.SpiRead(matrix_hal::kDACBaseAddress + 2051, (uint8_t *)(&write_pointer), sizeof(uint16_t));

  if (write_pointer > read_pointer)
    return write_pointer - read_pointer;
  else
    return fifoSize - read_pointer + write_pointer;
}