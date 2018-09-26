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

#include "everloop.h"
#include <unistd.h>
#include <iostream>
#include <string>
#include <valarray>
#include "voice_memory_map.h"

namespace matrix_hal {

Everloop::Everloop() {}

bool Everloop::Write(const EverloopImage* led_image) {
  if (!wishbone_) return false;

  std::valarray<unsigned char> write_data(led_image->leds.size() * 4);

  uint32_t led_offset = 0;
  for (const LedValue& led : led_image->leds) {
    write_data[led_offset + 0] = led.red;
    write_data[led_offset + 1] = led.green;
    write_data[led_offset + 2] = led.blue;
    write_data[led_offset + 3] = led.white;
    led_offset += 4;
  }

  wishbone_->SpiWrite(kEverloopBaseAddress, &write_data[0], write_data.size());
  return true;
}
};  // namespace matrix_hal
