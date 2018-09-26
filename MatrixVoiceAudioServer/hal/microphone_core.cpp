/*
 * Copyright 20178 <Admobilize>
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

#include <unistd.h>
#include <iostream>
#include <string>
#include <valarray>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "./voice_memory_map.h"
#include "./microphone_array.h"
#include "./microphone_array_location.h"
#include "./microphone_core.h"
#include "./microphone_core_fir.h"

namespace matrix_hal {

MicrophoneCore::MicrophoneCore(MicrophoneArray &mics) : mics_(mics) {
  fir_coeff_.resize(kNumberFIRTaps);
}

MicrophoneCore::~MicrophoneCore() {}

void MicrophoneCore::Setup(WishboneBus *wishbone) {
  MatrixDriver::Setup(wishbone);
  SelectFIRCoeff(&FIR_default[0]);
}

bool MicrophoneCore::SetFIRCoeff() {
  return wishbone_->SpiWrite(kMicrophoneArrayBaseAddress,
                             reinterpret_cast<uint8_t *>(&fir_coeff_[0]),
                             fir_coeff_.size());
}

bool MicrophoneCore::SetCustomFIRCoeff(
    const std::valarray<int16_t> custom_fir) {
  if (custom_fir.size() == kNumberFIRTaps) {
    fir_coeff_ = custom_fir;
    return SetFIRCoeff();
  } else {
    std::cerr << "Size FIR Filter must be : " << kNumberFIRTaps << std::endl;
    return false;
  }
}

bool MicrophoneCore::SelectFIRCoeff(FIRCoeff *FIR_coeff) {
  uint32_t sampling_frequency = mics_.SamplingRate();
  if (sampling_frequency == 0) {
    std::cerr << "Bad Configuration, sampling_frequency must be greather than 0"
              << std::endl;
    return false;
  }

  for (int i = 0;; i++) {
    if (FIR_coeff[i].rate_ == 0) {
      std::cerr << "Unsoported sampling frequency, it must be: 8000, 12000, "
                   "16000, 22050, 24000, 32000, 44100, 48000, 96000 "
                << std::endl;
      return false;
    }
    if (FIR_coeff[i].rate_ == sampling_frequency) {
      if (FIR_coeff[i].coeff_.size() == kNumberFIRTaps) {
        fir_coeff_ = FIR_coeff[i].coeff_;
        return SetFIRCoeff();
      } else {
        std::cerr << "Size FIR Filter must be : " << kNumberFIRTaps << "---"
                  << FIR_coeff[i].coeff_.size() << std::endl;
      }
    }
  }
}
};  // namespace matrix_hal
