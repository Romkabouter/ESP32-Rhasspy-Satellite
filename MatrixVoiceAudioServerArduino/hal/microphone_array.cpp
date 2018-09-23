/*
 * Copyright 2017 <Admobilize>
 * MATRIX Labs  [http://creator.matrix.one]
 * This file is part of MATRIX Voice HAL for ESP32
 *
 * Author:
 *       Andrés Calderón <andres.calderon@admobilize.com>
 *
 * MATRIX Voice ESP32 HAL is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <valarray>

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "./microphone_array.h"
#include "./microphone_array_location.h"
#include "./voice_memory_map.h"

static QueueHandle_t irq_queue;

static void irq_handler(void *args) {
  gpio_num_t gpio;
  gpio = static_cast<gpio_num_t>(matrix_hal::kMicrophoneArrayIRQ);
  xQueueSendToBackFromISR(irq_queue, &gpio, NULL);
}

namespace matrix_hal {

MicrophoneArray::MicrophoneArray() : gain_(3), sampling_frequency_(16000) {
  raw_data_.resize(kMicarrayBufferSize);

  delayed_data_.resize(kMicarrayBufferSize);

  fifos_.resize(kMicrophoneChannels);

  beamformed_.resize(NumberOfSamples());

  CalculateDelays(0.0, 0.0);

  irq_queue = xQueueCreate(10, sizeof(gpio_num_t));

  gpio_config_t gpioConfig;
  gpioConfig.pin_bit_mask = GPIO_SEL_5;
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
  gpioConfig.intr_type = GPIO_INTR_ANYEDGE;
  gpio_config(&gpioConfig);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(static_cast<gpio_num_t>(matrix_hal::kMicrophoneArrayIRQ),
                       irq_handler, NULL);
}

MicrophoneArray::~MicrophoneArray() {}

void MicrophoneArray::Setup(WishboneBus *wishbone) {
  MatrixDriver::Setup(wishbone);

  ReadConfValues();
}

//  Read audio from the FPGA and calculate beam using delay & sum method
bool MicrophoneArray::Read() {
  if (!wishbone_) return false;

  gpio_num_t gpio;
  gpio = static_cast<gpio_num_t>(matrix_hal::kMicrophoneArrayIRQ);

  xQueueReceive(irq_queue, &gpio, portMAX_DELAY);

  for (uint16_t c = 0; c < kMicrophoneChannels; c++) {
    if (wishbone_->SpiRead(kMicrophoneArrayBaseAddress + c * NumberOfSamples(),
                           reinterpret_cast<unsigned char *>(&raw_data_[0]),
                           sizeof(int16_t) * NumberOfSamples()) != ESP_OK) {
      return false;
    }
  }

  for (uint32_t s = 0; s < NumberOfSamples(); s++) {
    int sum = 0;
    for (uint16_t c = 0; c < kMicrophoneChannels; c++) {
      // delaying data for beamforming 'delay & sum' algorithm
      delayed_data_[c * NumberOfSamples() + s] =
          fifos_[c].PushPop(raw_data_[c * NumberOfSamples() + s]);

      // accumulation data for beamforming 'delay & sum' algorithm
      sum += delayed_data_[c * NumberOfSamples() + s];
    }

    beamformed_[s] = std::min(INT16_MAX, std::max(sum, INT16_MIN));
  }

  return true;
}

// Setting fifos for the 'delay & sum' algorithm
void MicrophoneArray::CalculateDelays(float azimutal_angle, float polar_angle,
                                      float radial_distance_mm,
                                      float sound_speed_mmseg) {
  //  sound source position
  float x, y, z;
  x = radial_distance_mm * std::sin(azimutal_angle) * std::cos(polar_angle);
  y = radial_distance_mm * std::sin(azimutal_angle) * std::sin(polar_angle);
  z = radial_distance_mm * std::cos(azimutal_angle);

  std::map<float, int> distance_map;

  // sorted distances from source position to each microphone
  for (int c = 0; c < kMicrophoneChannels; c++) {
    const float distance = std::sqrt(
        std::pow(micarray_location[c][0] - x, 2.0) +
        std::pow(micarray_location[c][1] - y, 2.0) + std::pow(z, 2.0));
    distance_map[distance] = c;
  }

  // fifo resize for delay compensation
  float min_distance = distance_map.begin()->first;
  for (std::map<float, int>::iterator it = distance_map.begin();
       it != distance_map.end(); ++it) {
    int delay = std::round((it->first - min_distance) * sampling_frequency_ /
                           sound_speed_mmseg);
    fifos_[it->second].Resize(delay);
  }
}

bool MicrophoneArray::GetGain() {
  if (!wishbone_) return false;
  uint16_t value;
  wishbone_->RegRead16(kConfBaseAddress + 0x07, &value);
  gain_ = value;
  return true;
}

bool MicrophoneArray::SetGain(uint16_t gain) {
  if (!wishbone_) return false;
  wishbone_->RegWrite16(kConfBaseAddress + 0x07, gain);
  gain_ = gain;
  return true;
}

bool MicrophoneArray::SetSamplingRate(uint32_t sampling_frequency) {
  if (sampling_frequency == 0) {
    return false;
  }

  uint16_t MIC_gain, MIC_constant;
  for (int i = 0;; i++) {
    if (MIC_sampling_frequencies[i][0] == 0) return false;
    if (sampling_frequency == MIC_sampling_frequencies[i][0]) {
      sampling_frequency_ = MIC_sampling_frequencies[i][0];
      MIC_constant = MIC_sampling_frequencies[i][1];
      MIC_gain = MIC_sampling_frequencies[i][2];
      break;
    }
  }

  sampling_frequency_ = sampling_frequency;
  SetGain(MIC_gain);
  wishbone_->RegWrite16(kConfBaseAddress + 0x06, MIC_constant);

  return true;
}

bool MicrophoneArray::GetSamplingRate() {
  if (!wishbone_) return false;
  uint16_t value;
  wishbone_->RegRead16(kConfBaseAddress + 0x06, &value);

  for (int i = 0;; i++) {
    if (MIC_sampling_frequencies[i][0] == 0) return false;
    if (value == MIC_sampling_frequencies[i][0]) {
      sampling_frequency_ = MIC_sampling_frequencies[i][0];
      break;
    }
  }

  return true;
}

void MicrophoneArray::ReadConfValues() {
  GetGain();
  GetSamplingRate();
}

};  // namespace matrix_hal
