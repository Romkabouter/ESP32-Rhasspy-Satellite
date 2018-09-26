/*
 * Copyright 2018 <Admobilize>
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

#ifndef CPP_DRIVER_MICROPHONE_CORE_H_
#define CPP_DRIVER_MICROPHONE_CORE_H_

#include <mutex>
#include <string>
#include <valarray>

#include "./circular_queue.h"
#include "./matrix_driver.h"

namespace matrix_hal {

struct FIRCoeff {
  uint32_t rate_;
  std::valarray<int16_t> coeff_;
};

const uint16_t kNumberFIRTaps = 128;

class MicrophoneCore : public MatrixDriver {
 public:
  MicrophoneCore(MicrophoneArray &mics);
  ~MicrophoneCore();
  void Setup(WishboneBus *wishbone);
  bool SetFIRCoeff();
  bool SetCustomFIRCoeff(const std::valarray<int16_t> custom_fir);
  bool SelectFIRCoeff(FIRCoeff *FIR_coeff);
  bool SetFIRCoeff(const std::valarray<int16_t> custom_fir);

 private:
  MicrophoneArray &mics_;
  std::valarray<int16_t> fir_coeff_;
};
};      // namespace matrix_hal
#endif  // CPP_DRIVER_MICROPHONE_ARRAY_H_
