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

#ifndef CPP_DRIVER_EVERLOOP_IMAGE_H_
#define CPP_DRIVER_EVERLOOP_IMAGE_H_

#include <cstdint>
#include <vector>

namespace matrix_hal {

class LedValue {
 public:
  LedValue() {
    red = 0;
    green = 0;
    blue = 0;
    white = 0;
  }

  void Set(uint32_t r, uint32_t g, uint32_t b, uint32_t w)
  {
    red = r;
    green = g;
    blue = b;
    white = w;
  }

  uint32_t red;
  uint32_t green;
  uint32_t blue;
  uint32_t white;
};

const int kMatrixCreatorNLeds = 18;

class EverloopImage {
 public:
  EverloopImage(int nleds = kMatrixCreatorNLeds) { leds.resize(nleds); }
  std::vector<LedValue> leds;
};

};      // namespace matrix_hal
#endif  // CPP_DRIVER_EVERLOOP_IMAGE_H_

