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
extern "C" {
  #include "speex_resampler.h"
}

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

int err;
SpeexResamplerState *resampler = speex_resampler_init(1, 16000, 16000, 0, &err);

class MatrixVoice : public Device
{
public:
  MatrixVoice();
  void init();
	void updateColors(int colors);
	void updateBrightness(int brightness);
  void muteOutput(bool mute);
  void setVolume(uint16_t volume);
  void setWriteMode(int sampleRate, int bitDepth, int numChannels); 
	bool readAudio(uint8_t *data, size_t size);
  void writeAudio(uint8_t *data, size_t size, size_t *bytes_written);
  void ampOutput(int output);
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
	uint16_t GetFIFOStatus();
	uint32_t PCM_sampling_frequency = 16000;
	int fifoSize = 4096;
  int sampleRate, bitDepth, numChannels;
	int brightness = 15;
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
};

void MatrixVoice::updateBrightness(int brightness) {
	// all values below 10 is read as 0 in gamma8, we map 0 to 10
	MatrixVoice::brightness = brightness * 90 / 100 + 10;
}

void MatrixVoice::updateColors(int colors) {
	int r = 0;
	int g = 0;
	int b = 0;
	int w = 0;	
  switch (colors) {
    case COLORS_HOTWORD:
			r = hotword_colors[0];
			g = hotword_colors[1];
			b = hotword_colors[2];
			w = hotword_colors[3];		
    break;
    case COLORS_WIFI_CONNECTED:
			r = wifi_conn_colors[0];
			g = wifi_conn_colors[1];
			b = wifi_conn_colors[2];
			w = wifi_conn_colors[3];		
    break;
    case COLORS_IDLE:
			r = idle_colors[0];
			g = idle_colors[1];
			b = idle_colors[2];
			w = idle_colors[3];		
    break;
    case COLORS_WIFI_DISCONNECTED:
			r = wifi_disc_colors[0];
			g = wifi_disc_colors[1];
			b = wifi_disc_colors[2];
			w = wifi_disc_colors[3];		
    break;
    case COLORS_OTA:
			r = ota_colors[0];
			g = ota_colors[1];
			b = ota_colors[2];
			w = ota_colors[3];		
    break;
  }
	r = floor(MatrixVoice::brightness * r / 100);
	r = pgm_read_byte(&gamma8[r]);
	g = floor(MatrixVoice::brightness * g / 100);
	g = pgm_read_byte(&gamma8[g]);
	b = floor(MatrixVoice::brightness * b / 100);
	b = pgm_read_byte(&gamma8[b]);
	w = floor(MatrixVoice::brightness * w / 100);
	w = pgm_read_byte(&gamma8[w]);
	for (matrix_hal::LedValue &led : image1d.leds) {
		led.red = r;
		led.green = g;
		led.blue = b;
		led.white = w;
	}
	everloop.Write(&image1d);
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

void MatrixVoice::setWriteMode(int sampleRate, int bitDepth, int numChannels) {
	MatrixVoice::sampleRate = sampleRate;
	MatrixVoice::bitDepth = bitDepth;
	MatrixVoice::numChannels = numChannels;
	FIFOFlush();
	speex_resampler_set_rate(resampler,sampleRate,16000);
	speex_resampler_skip_zeros(resampler);	
	if (numChannels == 1) {
		writeSize = 512;
	} else {
		writeSize = 1024;
	}
	if (sampleRate == 16000) {
		writeSize = 1024;
	}
	if (sampleRate == 44100) {
		writeSize = 2822;
	}
	if (sampleRate == 22050) {
		writeSize = 1411;
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

void MatrixVoice::writeAudio(uint8_t *data, size_t size, size_t *bytes_written) {
	*bytes_written = size;
	float sample_time = 1.0 / 16000;
	// std::valarray<uint16_t> output;
	// output.resize(size / sizeof(int16_t));
	uint32_t in_len = size / sizeof(int16_t);
	uint32_t out_len;
	int16_t input[in_len];
	int16_t output[512];
	int sleep = int(out_len * sizeof(int16_t) * sample_time * 1000);
	for (int i = 0; i < size; i += 2) {
			input[i/2] = ((data[i] & 0xff) | (data[i + 1] << 8));
	}
	//if (sampleRate != 16000) {
		speex_resampler_process_interleaved_int(resampler, input, &in_len, output, &out_len); 
	//}


  // if (numChannels == 1) {
	// 	// int16_t mono[size / sizeof(int16_t)]; 
	// 	// for (int i = 0; i < size; i += 2) {
	// 	// 		mono[i/2] = ((data[i] & 0xff) | (data[i + 1] << 8));
	// 	// }
	// 	// interleave(mono, mono, output, size / sizeof(int16_t));
	// } else {
	// 	// uint32_t in_len;
	// 	// uint32_t out_len;
	// 	// in_len = size;
	// 	// out_len = 512;
	// 	// int16_t output[out_len];
	// 	// int16_t input[in_len];
	// 	// //Convert 8 bit to 16 bit
	// 	// for (int i = 0; i < size; i += 2) {
	// 	// 		input[i/2] = ((data[i] & 0xff) | (data[i + 1] << 8));
	// 	// }
	// 	// speex_resampler_process_interleaved_int(resampler, input, &in_len, output, &out_len); 

	// 	// if (fifo_status > fifoSize * 3 / 4) {
	// 	// 	int sleep = int(size * sample_time * 1000);
	// 	// 	std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
	// 	// }
	// 	// wb.SpiWrite(matrix_hal::kDACBaseAddress, (const uint8_t *)data, size);
	// }

	uint16_t fifo_status = GetFIFOStatus();
	if (fifo_status > fifoSize * 3 / 4) {
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
	}
	wb.SpiWrite(matrix_hal::kDACBaseAddress, (const uint8_t *)output, out_len * sizeof(int16_t));
}

// void MatrixVoice::writeAudio(uint8_t *data, size_t size, size_t *bytes_written) {
// 	*bytes_written = size;
// 	uint32_t in_len;
// 	uint32_t out_len;
// 	in_len = size / sizeof(int16_t);
// 	out_len = size * (float)(16000 / sampleRate);
// 	int16_t output[out_len];
// 	int16_t input[in_len];
// 	//Convert 8 bit to 16 bit
// 	for (int i = 0; i < size; i += 2) {
// 			input[i/2] = ((data[i] & 0xff) | (data[i + 1] << 8));
// 	}

// 	if (MatrixVoice::numChannels == 2) {
// 			speex_resampler_process_interleaved_int(resampler, input, &in_len, output, &out_len); 
			
// 			//play it!
// 			playBytes(output, out_len);      
// 	} else {
// 			speex_resampler_process_int(resampler, 0, input, &in_len, output, &out_len);
// 			int16_t stereo[out_len * sizeof(int16_t)];
// 			int16_t mono[out_len];

// 			for (int i = 0; i < out_len; i++) {
// 					mono[i] = output[i];                               
// 			}
// 			MatrixVoice::interleave(mono, mono, stereo, out_len);

// 			//play it!                         
// 			MatrixVoice::playBytes(stereo, out_len * sizeof(int16_t));      
// 	} 
// };

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