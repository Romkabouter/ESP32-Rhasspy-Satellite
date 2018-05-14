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

#ifndef CPP_DRIVER_MICROPHONE_ARRAY_H_
#define CPP_DRIVER_MICROPHONE_ARRAY_H_

#include <string>
#include <valarray>
#include "./circular_queue.h"
#include "./matrix_driver.h"

namespace matrix_hal {

const uint32_t kPDMFrequency = 3000000;
const uint32_t kCICStages = 3;
const uint16_t kCICWidth = 23;
const uint16_t kMicarrayBufferSize = 2048;
const uint16_t kMicrophoneArrayIRQ = 5;
const uint16_t kMicrophoneChannels = 8;

class MicrophoneArray : public MatrixDriver {
 public:
  MicrophoneArray();

  ~MicrophoneArray();

  void Setup(WishboneBus* wishbone);
  bool Read();
  uint32_t SamplingRate() { return sampling_frequency_; }
  uint16_t DecimationRatio() { return decimation_ratio_; }
  uint16_t Gain() { return gain_; }
  bool GetDecimationRatio();
  bool GetPDMRatio();
  bool SetPDMRatio(uint16_t pdm_ratio);
  bool SetSamplingRate(uint32_t sampling_frequency);
  bool SetDecimationRatio(uint16_t decimation_counter);
  bool GetGain();
  bool SetGain(uint16_t gain);
  void ReadConfValues();
  uint16_t Channels() { return kMicrophoneChannels; }
  uint32_t NumberOfSamples() {
    return kMicarrayBufferSize / kMicrophoneChannels;
  }

  int16_t& At(int16_t sample, int16_t channel) {
    return delayed_data_[channel * NumberOfSamples() + sample];
  }

  int16_t& Beam(int16_t sample) { return beamformed_[sample]; }

  void CalculateDelays(float azimutal_angle, float polar_angle,
                       float radial_distance_mm = 100.0,
                       float sound_speed_mmseg = 320 * 1000.0);

 private:
  //  delay and sum beamforming result
  std::valarray<int16_t> beamformed_;
  std::valarray<int16_t> raw_data_;
  std::valarray<int16_t> delayed_data_;
  int16_t gain_;
  uint16_t pdm_ratio_;
  uint16_t sampling_frequency_;
  uint16_t decimation_ratio_;

  // beamforming delay and sum support
  std::valarray<CircularQueue<int16_t> > fifos_;
};
};      // namespace matrix_hal
#endif  // CPP_DRIVER_MICROPHONE_ARRAY_H_
