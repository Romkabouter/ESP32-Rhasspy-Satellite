/*
 * Copyright 2016 <Admobilize>
 * MATRIX Labs  [http://creator.matrix.one]
 * This file is part of MATRIX Creator HAL
 *
 * MATRIX Creator HAL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CPP_DRIVER_AUDIO_OUTPUT_H_
#define CPP_DRIVER_AUDIO_OUTPUT_H_

#include <string>
#include "./matrix_driver.h"

namespace matrix_hal {

const uint32_t kDACFrequency = 4000000;
const uint16_t kMaxVolumenValue = 25;
const uint32_t kFIFOSize = 4096;
const uint32_t kMaxWriteLength = 1024;

const uint32_t PCM_sampling_frequencies[][2] = {
    {8000, 975},  {16000, 492}, {32000, 245}, {44100, 177},
    {48000, 163}, {88200, 88},  {96000, 81},  {0, 0}};

enum MuteStatus : uint16_t { kMute = 0x0001, kUnMute = 0x0000 };

enum OutputSelector : uint16_t {
  kHeadPhone = 0x0001,
  kSpeaker = 0x0000

};

class AudioOutput : public MatrixDriver {
 public:
  AudioOutput();

  ~AudioOutput();

  void Setup(WishboneBus *wishbone);

  bool Mute();
  bool UnMute();

  bool SetOutputSelector(OutputSelector output_selector);

  bool FIFOFlush();
  bool GetPCMSamplingFrequency();
  bool SetPCMSamplingFrequency(uint32_t PCM_sampling_frequency);
  uint32_t PCMSamplingFrequency() { return PCM_sampling_frequency_; }

  void Write();

  uint16_t GetFIFOStatus();

  bool SetVolumen(int volumen_percentage);

  std::valarray<uint16_t> write_data_;

 private:
  MuteStatus mute_status_;
  int16_t volumen_percentage_;
  OutputSelector selector_hp_nspk_;
  uint32_t PCM_sampling_frequency_;
};
};      // namespace matrix_hal
#endif  // CPP_DRIVER_AUDIO_OUTPUT_H_
